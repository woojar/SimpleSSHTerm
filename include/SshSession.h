#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

class SshSession : public QObject {
  Q_OBJECT
public:
  explicit SshSession(QObject *parent = nullptr);
  ~SshSession();

  void connectToHost(const QString &host,
                     const QString &user,
                     const QString &password,
                     const QString &keyPath,
                     const QString &keyPassphrase,
                     int port = 22);
  void send(const QByteArray &data);
  void disconnectFromHost();
  void setPtySize(int rows, int cols);

signals:
  void output(const QByteArray &data);
  void error(const QString &message);
  void connected();
  void disconnected();

private:
  struct Impl;
  Impl *impl_;
  bool connected_ = false;
};
