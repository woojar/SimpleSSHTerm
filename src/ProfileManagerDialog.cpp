#include "ProfileManagerDialog.h"

#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegExp>
#include <QSet>
#include <QSpinBox>
#include <QSettings>
#include <QCheckBox>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>

ProfileManagerDialog::ProfileManagerDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle("Profiles");
  resize(520, 360);

  list_ = new QListWidget(this);
  connect(list_, &QListWidget::currentRowChanged, this, &ProfileManagerDialog::onSelectionChanged);

  name_ = new QLineEdit(this);
  host_ = new QLineEdit(this);
  user_ = new QLineEdit(this);
  port_ = new QSpinBox(this);
  port_->setRange(1, 65535);
  port_->setValue(22);

  keyPath_ = new QLineEdit(this);
  browseKeyButton_ = new QToolButton(this);
  browseKeyButton_->setText("Browse");
  connect(browseKeyButton_, &QToolButton::clicked, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(this, "Select Private Key", QDir::homePath() + "/.ssh");
    if (!path.isEmpty()) {
      keyPath_->setText(path);
    }
  });

  auto *form = new QFormLayout();
  form->addRow("Name", name_);
  form->addRow("Host", host_);
  form->addRow("User", user_);
  form->addRow("Port", port_);

  auto *keyRow = new QHBoxLayout();
  keyRow->addWidget(keyPath_);
  keyRow->addWidget(browseKeyButton_);
  auto *keyRowWrap = new QWidget(this);
  keyRowWrap->setLayout(keyRow);
  form->addRow("Key Path", keyRowWrap);

  addButton_ = new QPushButton("Add", this);
  saveButton_ = new QPushButton("Save", this);
  deleteButton_ = new QPushButton("Delete", this);
  importButton_ = new QPushButton("Import SSH Config", this);
  connectButton_ = new QPushButton("Connect", this);

  connect(addButton_, &QPushButton::clicked, this, &ProfileManagerDialog::onAddProfile);
  connect(saveButton_, &QPushButton::clicked, this, &ProfileManagerDialog::onSaveProfile);
  connect(deleteButton_, &QPushButton::clicked, this, &ProfileManagerDialog::onDeleteProfile);
  connect(importButton_, &QPushButton::clicked, this, &ProfileManagerDialog::onImportSshConfig);
  connect(connectButton_, &QPushButton::clicked, this, &ProfileManagerDialog::onConnect);

  auto *buttonsRow = new QHBoxLayout();
  buttonsRow->addWidget(addButton_);
  buttonsRow->addWidget(saveButton_);
  buttonsRow->addWidget(deleteButton_);
  buttonsRow->addWidget(importButton_);
  buttonsRow->addStretch(1);
  buttonsRow->addWidget(connectButton_);

  protectCheck_ = new QCheckBox("Protect profiles with passphrase", this);
  QSettings settings("sshterminal", "sshterminal");
  protectCheck_->setChecked(settings.value("profiles/encrypted", false).toBool());

  openInNewTabCheck_ = new QCheckBox("Open this profile in new tab by default", this);

  auto *layout = new QVBoxLayout();
  layout->addWidget(list_);
  layout->addLayout(form);
  layout->addWidget(protectCheck_);
  layout->addWidget(openInNewTabCheck_);
  layout->addLayout(buttonsRow);
  setLayout(layout);

  if (!loadProfiles()) {
    // If user cancels passphrase, close dialog.
    reject();
    return;
  }

  refreshList();
  if (!profiles_.isEmpty()) {
    list_->setCurrentRow(0);
  }
}

const Profile &ProfileManagerDialog::selectedProfile() const {
  return selected_;
}

void ProfileManagerDialog::onSelectionChanged() {
  const int idx = currentIndex();
  if (idx < 0 || idx >= profiles_.size()) {
    return;
  }
  setFieldsFromProfile(profiles_.at(idx));
}

void ProfileManagerDialog::onAddProfile() {
  Profile p = profileFromFields();
  if (p.name.trimmed().isEmpty()) {
    QMessageBox::warning(this, "Profile", "Name is required");
    return;
  }
  profiles_.push_back(p);
  refreshList();
  list_->setCurrentRow(profiles_.size() - 1);
  saveProfiles();
}

void ProfileManagerDialog::onSaveProfile() {
  const int idx = currentIndex();
  if (idx < 0 || idx >= profiles_.size()) {
    QMessageBox::warning(this, "Profile", "Select a profile to save");
    return;
  }
  Profile p = profileFromFields();
  if (p.name.trimmed().isEmpty()) {
    QMessageBox::warning(this, "Profile", "Name is required");
    return;
  }
  profiles_[idx] = p;
  refreshList();
  list_->setCurrentRow(idx);
  saveProfiles();
}

void ProfileManagerDialog::onDeleteProfile() {
  const int idx = currentIndex();
  if (idx < 0 || idx >= profiles_.size()) {
    return;
  }
  profiles_.removeAt(idx);
  refreshList();
  if (!profiles_.isEmpty()) {
    list_->setCurrentRow(qMin(idx, profiles_.size() - 1));
  }
  saveProfiles();
}

void ProfileManagerDialog::onConnect() {
  const int idx = currentIndex();
  if (idx < 0 || idx >= profiles_.size()) {
    QMessageBox::warning(this, "Profile", "Select a profile to connect");
    return;
  }
  selected_ = profileFromFields();
  profiles_[idx] = selected_;
  saveProfiles();
  accept();
}

static bool isUsableHostAlias(const QString &alias) {
  return !(alias.contains('*') || alias.contains('?'));
}

void ProfileManagerDialog::onImportSshConfig() {
  const QString configPath = QDir::homePath() + "/.ssh/config";
  QFile f(configPath);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(this, "Import SSH Config", "Failed to open ~/.ssh/config");
    return;
  }

  QSet<QString> existing;
  for (const auto &p : profiles_) {
    existing.insert(p.name.trimmed());
  }

  QVector<Profile> imported;
  QTextStream in(&f);
  QString currentAlias;
  Profile currentProfile;
  bool inHostBlock = false;

  auto flushCurrent = [&]() {
    if (!inHostBlock) {
      return;
    }
    if (currentAlias.isEmpty()) {
      return;
    }
    currentProfile.name = currentAlias;
    if (currentProfile.host.isEmpty()) {
      currentProfile.host = currentAlias;
    }
    if (currentProfile.user.isEmpty()) {
      currentProfile.user = qgetenv("USER");
    }
    if (!existing.contains(currentProfile.name)) {
      imported.push_back(currentProfile);
      existing.insert(currentProfile.name);
    }
  };

  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.isEmpty() || line.startsWith('#')) {
      continue;
    }

    const int commentIdx = line.indexOf('#');
    if (commentIdx >= 0) {
      line = line.left(commentIdx).trimmed();
    }

    const QStringList parts = line.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
      continue;
    }

    const QString key = parts.at(0).toLower();
    if (key == "host") {
      flushCurrent();

      currentAlias.clear();
      currentProfile = Profile();
      inHostBlock = false;

      for (int i = 1; i < parts.size(); ++i) {
        const QString alias = parts.at(i);
        if (isUsableHostAlias(alias)) {
          currentAlias = alias;
          inHostBlock = true;
          break;
        }
      }
      continue;
    }

    if (!inHostBlock) {
      continue;
    }

    if (key == "hostname" && parts.size() >= 2) {
      currentProfile.host = parts.at(1);
    } else if (key == "user" && parts.size() >= 2) {
      currentProfile.user = parts.at(1);
    } else if (key == "port" && parts.size() >= 2) {
      currentProfile.port = parts.at(1).toInt();
      if (currentProfile.port <= 0) {
        currentProfile.port = 22;
      }
    }
  }

  flushCurrent();

  if (imported.isEmpty()) {
    QMessageBox::information(this, "Import SSH Config", "No new host entries found.");
    return;
  }

  for (const auto &p : imported) {
    profiles_.push_back(p);
  }
  refreshList();
  saveProfiles();

  QMessageBox::information(this, "Import SSH Config", QString("Imported %1 profile(s).").arg(imported.size()));
}

bool ProfileManagerDialog::loadProfiles() {
  const QString path = storePath();
  const QFileInfo fi(path);
  const bool exists = fi.exists();

  ProfileStore store(path);
  if (!exists) {
    profiles_.clear();
    return true;
  }

  QSettings settings("sshterminal", "sshterminal");
  const bool protect = settings.value("profiles/encrypted", false).toBool();
  QString error;

  QFile f(path);
  QByteArray raw;
  bool isEncrypted = false;
  if (f.open(QIODevice::ReadOnly)) {
    raw = f.readAll();
    isEncrypted = ProfileStore::looksEncrypted(raw);
  }

  if (protect) {
    if (isEncrypted) {
      if (!promptPassphrase(true)) {
        return false;
      }
      if (!store.loadEncrypted(&profiles_, passphrase_, &error)) {
        QMessageBox::warning(this, "Profiles", "Failed to unlock profiles: " + error);
        if (!promptPassphrase(false)) {
          return false;
        }
        return store.loadEncrypted(&profiles_, passphrase_, &error);
      }
      return true;
    }
    // Protected toggle on, but data is plain. Load plain without prompting.
    if (!store.loadPlain(&profiles_, &error)) {
      QMessageBox::warning(this, "Profiles", "Failed to load profiles: " + error);
      return false;
    }
    return true;
  }

  if (isEncrypted) {
    if (!promptPassphrase(true)) {
      return false;
    }
    if (!store.loadEncrypted(&profiles_, passphrase_, &error)) {
      QMessageBox::warning(this, "Profiles", "Failed to unlock profiles: " + error);
      return false;
    }
    store.savePlain(profiles_, &error);
    return true;
  }

  if (!store.loadPlain(&profiles_, &error)) {
    QMessageBox::warning(this, "Profiles", "Failed to load profiles: " + error);
    return false;
  }

  return true;
}

bool ProfileManagerDialog::saveProfiles() {
  ProfileStore store(storePath());
  QString error;
  if (protectCheck_->isChecked()) {
    if (passphrase_.isEmpty()) {
      if (!promptPassphrase(true)) {
        return false;
      }
    }
    if (!store.saveEncrypted(profiles_, passphrase_, &error)) {
      QMessageBox::warning(this, "Profiles", "Failed to save profiles: " + error);
      return false;
    }
  } else {
    if (!store.savePlain(profiles_, &error)) {
      QMessageBox::warning(this, "Profiles", "Failed to save profiles: " + error);
      return false;
    }
  }

  QSettings settings("sshterminal", "sshterminal");
  settings.setValue("profiles/encrypted", protectCheck_->isChecked());
  return true;
}

bool ProfileManagerDialog::promptPassphrase(bool confirmIfNew) {
  bool ok = false;
  QString pass = QInputDialog::getText(this, "Unlock Profiles", "Passphrase", QLineEdit::Password, "", &ok);
  if (!ok) {
    return false;
  }
  if (confirmIfNew) {
    bool ok2 = false;
    QString pass2 = QInputDialog::getText(this, "Create Profile Store", "Confirm passphrase", QLineEdit::Password, "", &ok2);
    if (!ok2 || pass != pass2) {
      QMessageBox::warning(this, "Profiles", "Passphrases do not match");
      return promptPassphrase(confirmIfNew);
    }
  }
  passphrase_ = pass;
  return true;
}

void ProfileManagerDialog::refreshList() {
  list_->clear();
  for (const auto &p : profiles_) {
    list_->addItem(p.name);
  }
}

void ProfileManagerDialog::setFieldsFromProfile(const Profile &p) {
  name_->setText(p.name);
  host_->setText(p.host);
  user_->setText(p.user);
  port_->setValue(p.port);
  keyPath_->setText(p.keyPath);
  openInNewTabCheck_->setChecked(p.openInNewTab);
}

Profile ProfileManagerDialog::profileFromFields() const {
  Profile p;
  p.name = name_->text();
  p.host = host_->text();
  p.user = user_->text();
  p.port = port_->value();
  p.keyPath = keyPath_->text();
  p.openInNewTab = openInNewTabCheck_->isChecked();
  return p;
}

int ProfileManagerDialog::currentIndex() const {
  return list_->currentRow();
}

QString ProfileManagerDialog::storePath() const {
  return ProfileStore::defaultPath();
}
