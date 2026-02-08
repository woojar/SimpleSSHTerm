#include "TerminalTab.h"
#include "ProfileManagerDialog.h"
#include "SshSession.h"
#include "TerminalWidget.h"

#include <QHBoxLayout>
#include <QFileInfo>
#include <QInputDialog>
#include <QPushButton>
#include <QVBoxLayout>

TerminalTab::TerminalTab(QWidget *parent)
    : QWidget(parent), terminal_(new TerminalWidget(this)), session_(new SshSession(this)) {
  auto *connectButton = new QPushButton("Connect", this);
  connect(connectButton, &QPushButton::clicked, this, &TerminalTab::onConnectClicked);

  connect(session_, &SshSession::output, this, &TerminalTab::onSessionOutput);
  connect(session_, &SshSession::error, this, &TerminalTab::onSessionError);
  connect(session_, &SshSession::connected, this, &TerminalTab::onSessionConnected);
  connect(session_, &SshSession::disconnected, this, &TerminalTab::onSessionDisconnected);

  connect(terminal_, &TerminalWidget::sendData, session_, &SshSession::send);
  connect(terminal_, &TerminalWidget::terminalResized, this, &TerminalTab::onTerminalResize);

  auto *topRowWidget = new QWidget(this);
  auto *topRow = new QHBoxLayout(topRowWidget);
  topRow->setContentsMargins(4, 2, 4, 2);
  topRow->setSpacing(6);
  connectButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  connectButton->setMinimumHeight(22);
  topRow->addWidget(connectButton);
  topRow->addStretch(1);
  topRowWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  topRowWidget->setFixedHeight(26);

  auto *layout = new QVBoxLayout();
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);
  layout->addWidget(topRowWidget);
  layout->addWidget(terminal_);
  setLayout(layout);
}

void TerminalTab::connectProfile(const Profile &p, bool promptKeyPass) {
  QString keyPass;
  const QString keyPath = p.keyPath.trimmed();
  if (promptKeyPass && !keyPath.isEmpty() && QFileInfo::exists(keyPath)) {
    bool ok = false;
    keyPass = QInputDialog::getText(this, "Key Passphrase", "Passphrase (leave empty if none)",
                                    QLineEdit::Password, "", &ok);
    if (!ok) {
      return;
    }
  }

  currentProfile_ = p;
  hasProfile_ = true;
  emit profileSelected(currentProfile_);
  session_->connectToHost(p.host, p.user, QString(), keyPath, keyPass, p.port);
}

bool TerminalTab::hasProfile() const {
  return hasProfile_;
}

bool TerminalTab::isConnected() const {
  return connected_;
}

Profile TerminalTab::currentProfile() const {
  return currentProfile_;
}

void TerminalTab::applyTheme(const QColor &fg, const QColor &bg, const QFont &font) {
  terminal_->setTheme(fg, bg, font);
}

void TerminalTab::onConnectClicked() {
  ProfileManagerDialog dlg(this);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  const Profile &p = dlg.selectedProfile();
  if (p.openInNewTab) {
    emit connectInNewTab(p);
    return;
  }
  connectProfile(p, true);
}

void TerminalTab::onSessionOutput(const QByteArray &data) {
  terminal_->writeData(data);
}

void TerminalTab::onSessionError(const QString &message) {
  terminal_->writeData("[Error] " + message.toUtf8() + "\n");
}

void TerminalTab::onTerminalResize(int rows, int cols) {
  session_->setPtySize(rows, cols);
}

void TerminalTab::onSessionConnected() {
  connected_ = true;
  terminal_->clearScreen();
  if (hasProfile_) {
    emit profileConnected(currentProfile_);
  }
}

void TerminalTab::onSessionDisconnected() {
  connected_ = false;
  emit requestClose();
}
