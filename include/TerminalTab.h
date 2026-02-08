#pragma once

#include <QWidget>
#include <QColor>
#include <QFont>
#include "ProfileStore.h"

class TerminalWidget;
class SshSession;

class TerminalTab : public QWidget {
  Q_OBJECT
public:
  explicit TerminalTab(QWidget *parent = nullptr);
  void connectProfile(const Profile &p, bool promptKeyPass = true);
  bool hasProfile() const;
  bool isConnected() const;
  Profile currentProfile() const;
  void applyTheme(const QColor &fg, const QColor &bg, const QFont &font);

signals:
  void profileConnected(const Profile &p);
  void profileSelected(const Profile &p);
  void connectInNewTab(const Profile &p);
  void requestClose();

private slots:
  void onConnectClicked();
  void onSessionOutput(const QByteArray &data);
  void onSessionError(const QString &message);
  void onTerminalResize(int rows, int cols);
  void onSessionConnected();
  void onSessionDisconnected();

private:
  TerminalWidget *terminal_;
  SshSession *session_;
  Profile currentProfile_;
  bool hasProfile_ = false;
  bool connected_ = false;
};
