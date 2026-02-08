#include "MainWindow.h"
#include "TerminalTab.h"
#include "ThemeDialog.h"

#include <QAction>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QFile>
#include <QSettings>
#include <QTabWidget>
#include <QTabBar>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), tabs_(new QTabWidget(this)) {
  tabs_->setTabBarAutoHide(false);
  tabs_->setUsesScrollButtons(true);
  tabs_->tabBar()->setElideMode(Qt::ElideRight);
  tabs_->tabBar()->setExpanding(false);
  tabs_->setTabsClosable(true);
  setCentralWidget(tabs_);

  auto *fileMenu = menuBar()->addMenu("File");
  auto *newTabAction = fileMenu->addAction("New Tab");
  newTabAction->setShortcut(QKeySequence::AddTab);
  connect(newTabAction, &QAction::triggered, this, &MainWindow::newTab);

  auto *closeTabAction = fileMenu->addAction("Close Tab");
  closeTabAction->setShortcut(QKeySequence::Close);
  connect(closeTabAction, &QAction::triggered, [this]() {
    closeTab(tabs_->currentIndex());
  });

  connect(tabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

  auto *viewMenu = menuBar()->addMenu("View");
  auto *themeAction = viewMenu->addAction("Theme...");
  connect(themeAction, &QAction::triggered, [this]() {
    ThemeDialog dlg(themeFg_, themeBg_, themeFont_, this);
    if (dlg.exec() == QDialog::Accepted) {
      themeFg_ = dlg.foreground();
      themeBg_ = dlg.background();
      themeFont_ = dlg.font();
      saveTheme();
      applyThemeToAll();
    }
  });

  loadTheme();

  if (!restoreSessions()) {
    newTab();
  }
}

void MainWindow::newTab() {
  auto *tab = new TerminalTab(this);
  int index = tabs_->addTab(tab, "Session");
  tabs_->setCurrentIndex(index);
  connect(tab, &TerminalTab::profileConnected, this, &MainWindow::onProfileConnected);
  connect(tab, &TerminalTab::profileSelected, this, &MainWindow::onProfileSelected);
  connect(tab, &TerminalTab::connectInNewTab, this, &MainWindow::onConnectInNewTab);
  connect(tab, &TerminalTab::requestClose, this, [this, tab]() {
    if (closing_) {
      return;
    }
    if (!tabs_) {
      return;
    }
    int idx = tabs_->indexOf(tab);
    if (idx >= 0) {
      closeTab(idx);
    }
  });
  tab->applyTheme(themeFg_, themeBg_, themeFont_);
}

void MainWindow::closeTab(int index) {
  if (index < 0) {
    return;
  }
  QWidget *w = tabs_->widget(index);
  tabs_->removeTab(index);
  delete w;

  if (tabs_->count() == 0) {
    newTab();
  }
}

void MainWindow::closeEvent(QCloseEvent *event) {
  closing_ = true;
  // Prevent session callbacks from closing tabs during teardown.
  if (tabs_) {
    tabs_->blockSignals(true);
    for (int i = 0; i < tabs_->count(); ++i) {
      auto *tab = qobject_cast<TerminalTab *>(tabs_->widget(i));
      if (tab) {
        tab->blockSignals(true);
        disconnect(tab, &TerminalTab::requestClose, this, nullptr);
      }
    }
  }
  QStringList names;
  for (int i = 0; i < tabs_->count(); ++i) {
    auto *tab = qobject_cast<TerminalTab *>(tabs_->widget(i));
    if (!tab) {
      continue;
    }
    if (tab->hasProfile()) {
      names.append(tab->currentProfile().name);
    }
  }
  saveLastSessions(names);
  QMainWindow::closeEvent(event);
}

void MainWindow::onProfileConnected(const Profile &p) {
  QStringList names = loadLastSessions();
  names.removeAll(p.name);
  names.prepend(p.name);
  while (names.size() > 10) {
    names.removeLast();
  }
  saveLastSessions(names);
}

void MainWindow::onProfileSelected(const Profile &p) {
  auto *tab = qobject_cast<TerminalTab *>(sender());
  if (!tab) {
    return;
  }
  const int index = tabs_->indexOf(tab);
  if (index < 0) {
    return;
  }
  tabs_->setTabText(index, p.name.isEmpty() ? "Session" : p.name);
}

void MainWindow::onConnectInNewTab(const Profile &p) {
  openTabWithProfile(p, true);
}

bool MainWindow::restoreSessions() {
  const QStringList names = loadLastSessions();
  if (names.isEmpty()) {
    return false;
  }

  const auto answer = QMessageBox::question(this, "Reconnect",
                                            "Reconnect last sessions?",
                                            QMessageBox::Yes | QMessageBox::No);
  if (answer != QMessageBox::Yes) {
    return false;
  }

  QVector<Profile> profiles;
  ProfileStore store(ProfileStore::defaultPath());
  QString error;
  QSettings settings("sshterminal", "sshterminal");
  const bool protect = settings.value("profiles/encrypted", false).toBool();
  const QString path = ProfileStore::defaultPath();
  QFile f(path);
  QByteArray raw;
  bool isEncrypted = false;
  if (f.open(QIODevice::ReadOnly)) {
    raw = f.readAll();
    isEncrypted = ProfileStore::looksEncrypted(raw);
  }

  if (protect || isEncrypted) {
    bool ok = false;
    const QString passphrase = QInputDialog::getText(this, "Unlock Profiles", "Passphrase",
                                                     QLineEdit::Password, "", &ok);
    if (!ok) {
      return false;
    }
    if (!store.loadEncrypted(&profiles, passphrase, &error)) {
      QMessageBox::warning(this, "Profiles", "Failed to unlock profiles: " + error);
      return false;
    }
  } else {
    if (!store.loadPlain(&profiles, &error)) {
      QMessageBox::warning(this, "Profiles", "Failed to load profiles: " + error);
      return false;
    }
  }

  QHash<QString, Profile> byName;
  for (const auto &p : profiles) {
    byName.insert(p.name, p);
  }

  bool restoredAny = false;
  for (const auto &name : names) {
    if (!byName.contains(name)) {
      continue;
    }
    openTabWithProfile(byName.value(name), true);
    restoredAny = true;
  }
  return restoredAny;
}

void MainWindow::openTabWithProfile(const Profile &p, bool autoConnect) {
  auto *tab = new TerminalTab(this);
  int index = tabs_->addTab(tab, p.name.isEmpty() ? "Session" : p.name);
  tabs_->setCurrentIndex(index);
  connect(tab, &TerminalTab::profileConnected, this, &MainWindow::onProfileConnected);
  connect(tab, &TerminalTab::profileSelected, this, &MainWindow::onProfileSelected);
  connect(tab, &TerminalTab::connectInNewTab, this, &MainWindow::onConnectInNewTab);
  connect(tab, &TerminalTab::requestClose, this, [this, tab]() {
    if (closing_) {
      return;
    }
    if (!tabs_) {
      return;
    }
    int idx = tabs_->indexOf(tab);
    if (idx >= 0) {
      closeTab(idx);
    }
  });
  tab->applyTheme(themeFg_, themeBg_, themeFont_);
  if (autoConnect) {
    tab->connectProfile(p, true);
  }
}

QStringList MainWindow::loadLastSessions() const {
  QSettings settings("sshterminal", "sshterminal");
  return settings.value("lastSessions").toStringList();
}

void MainWindow::saveLastSessions(const QStringList &names) const {
  QSettings settings("sshterminal", "sshterminal");
  settings.setValue("lastSessions", names);
}

void MainWindow::loadTheme() {
  QSettings settings("sshterminal", "sshterminal");
  themeFg_ = settings.value("theme/fg", QColor(220, 220, 220)).value<QColor>();
  themeBg_ = settings.value("theme/bg", QColor(0, 0, 0)).value<QColor>();
  themeFont_ = settings.value("theme/font", QFontDatabase::systemFont(QFontDatabase::FixedFont)).value<QFont>();
  if (themeFont_.pointSize() <= 0) {
    themeFont_.setPointSize(12);
  }
}

void MainWindow::saveTheme() const {
  QSettings settings("sshterminal", "sshterminal");
  settings.setValue("theme/fg", themeFg_);
  settings.setValue("theme/bg", themeBg_);
  settings.setValue("theme/font", themeFont_);
}

void MainWindow::applyThemeToAll() {
  for (int i = 0; i < tabs_->count(); ++i) {
    auto *tab = qobject_cast<TerminalTab *>(tabs_->widget(i));
    if (!tab) {
      continue;
    }
    tab->applyTheme(themeFg_, themeBg_, themeFont_);
  }
}
