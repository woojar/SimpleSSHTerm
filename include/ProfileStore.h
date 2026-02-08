#pragma once

#include <QString>
#include <QVector>

struct Profile {
  QString name;
  QString host;
  QString user;
  int port = 22;
  QString keyPath;
  bool openInNewTab = false;
};

class ProfileStore {
public:
  explicit ProfileStore(const QString &path);
  static QString defaultPath();

  bool savePlain(const QVector<Profile> &profiles, QString *error = nullptr) const;
  bool loadPlain(QVector<Profile> *profiles, QString *error = nullptr) const;
  bool saveEncrypted(const QVector<Profile> &profiles, const QString &passphrase, QString *error = nullptr) const;
  bool loadEncrypted(QVector<Profile> *profiles, const QString &passphrase, QString *error = nullptr) const;
  static bool looksEncrypted(const QByteArray &data);

private:
  QString path_;
};
