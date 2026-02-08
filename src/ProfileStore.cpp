#include "ProfileStore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>

#ifdef HAVE_SODIUM
#include <sodium.h>
#endif

static QJsonArray profilesToJson(const QVector<Profile> &profiles) {
  QJsonArray arr;
  for (const auto &p : profiles) {
    QJsonObject o;
    o["name"] = p.name;
    o["host"] = p.host;
    o["user"] = p.user;
    o["port"] = p.port;
    o["keyPath"] = p.keyPath;
    o["openInNewTab"] = p.openInNewTab;
    arr.push_back(o);
  }
  return arr;
}

static QVector<Profile> profilesFromJson(const QJsonArray &arr) {
  QVector<Profile> profiles;
  profiles.reserve(arr.size());
  for (const auto &v : arr) {
    const auto o = v.toObject();
    Profile p;
    p.name = o.value("name").toString();
    p.host = o.value("host").toString();
    p.user = o.value("user").toString();
    p.port = o.value("port").toInt(22);
    p.keyPath = o.value("keyPath").toString();
    p.openInNewTab = o.value("openInNewTab").toBool(false);
    profiles.push_back(p);
  }
  return profiles;
}

ProfileStore::ProfileStore(const QString &path) : path_(path) {}

QString ProfileStore::defaultPath() {
  const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(base);
  return base + "/profiles.json";
}

bool ProfileStore::savePlain(const QVector<Profile> &profiles, QString *error) const {
  QJsonArray arr = profilesToJson(profiles);
  QFile f(path_);
  if (!f.open(QIODevice::WriteOnly)) {
    if (error) *error = "failed to open profile store";
    return false;
  }
  f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
  return true;
}

bool ProfileStore::loadPlain(QVector<Profile> *profiles, QString *error) const {
  QFile f(path_);
  if (!f.open(QIODevice::ReadOnly)) {
    if (error) *error = "failed to open profile store";
    return false;
  }
  const auto doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isArray()) {
    if (error) *error = "invalid profile store";
    return false;
  }
  if (profiles) {
    *profiles = profilesFromJson(doc.array());
  }
  return true;
}

bool ProfileStore::looksEncrypted(const QByteArray &data) {
  const auto doc = QJsonDocument::fromJson(data);
  if (!doc.isObject()) {
    return false;
  }
  const auto obj = doc.object();
  return obj.contains("ciphertext") && obj.contains("salt") && obj.contains("nonce");
}

bool ProfileStore::saveEncrypted(const QVector<Profile> &profiles, const QString &passphrase, QString *error) const {
#ifdef HAVE_SODIUM
  if (sodium_init() < 0) {
    if (error) *error = "libsodium init failed";
    return false;
  }

  QJsonObject root;
  const QJsonArray arr = profilesToJson(profiles);
  const QByteArray plaintext = QJsonDocument(arr).toJson(QJsonDocument::Compact);

  QByteArray salt(crypto_pwhash_SALTBYTES, '\0');
  randombytes_buf(salt.data(), salt.size());

  QByteArray key(crypto_secretbox_KEYBYTES, '\0');
  const QByteArray passBytes = passphrase.toUtf8();
  if (crypto_pwhash(reinterpret_cast<unsigned char *>(key.data()),
                    static_cast<unsigned long long>(key.size()),
                    passBytes.constData(),
                    static_cast<unsigned long long>(passBytes.size()),
                    reinterpret_cast<const unsigned char *>(salt.constData()),
                    crypto_pwhash_OPSLIMIT_MODERATE, crypto_pwhash_MEMLIMIT_MODERATE,
                    crypto_pwhash_ALG_DEFAULT) != 0) {
    if (error) *error = "password hashing failed";
    return false;
  }

  QByteArray nonce(crypto_secretbox_NONCEBYTES, '\0');
  randombytes_buf(nonce.data(), nonce.size());

  QByteArray ciphertext(plaintext.size() + crypto_secretbox_MACBYTES, '\0');
  if (crypto_secretbox_easy(reinterpret_cast<unsigned char *>(ciphertext.data()),
                            reinterpret_cast<const unsigned char *>(plaintext.constData()),
                            plaintext.size(),
                            reinterpret_cast<const unsigned char *>(nonce.constData()),
                            reinterpret_cast<const unsigned char *>(key.constData())) != 0) {
    if (error) *error = "encryption failed";
    return false;
  }

  root["kdf"] = "crypto_pwhash";
  root["salt"] = QString::fromUtf8(salt.toBase64());
  root["nonce"] = QString::fromUtf8(nonce.toBase64());
  root["ciphertext"] = QString::fromUtf8(ciphertext.toBase64());

  QFile f(path_);
  if (!f.open(QIODevice::WriteOnly)) {
    if (error) *error = "failed to open profile store";
    return false;
  }
  f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return true;
#else
  Q_UNUSED(profiles)
  Q_UNUSED(passphrase)
  if (error) *error = "libsodium not available at build time";
  return false;
#endif
}

bool ProfileStore::loadEncrypted(QVector<Profile> *profiles, const QString &passphrase, QString *error) const {
#ifdef HAVE_SODIUM
  if (sodium_init() < 0) {
    if (error) *error = "libsodium init failed";
    return false;
  }

  QFile f(path_);
  if (!f.open(QIODevice::ReadOnly)) {
    if (error) *error = "failed to open profile store";
    return false;
  }

  const QByteArray raw = f.readAll();
  const auto doc = QJsonDocument::fromJson(raw);
  if (!doc.isObject()) {
    if (error) *error = "invalid profile store";
    return false;
  }

  const auto root = doc.object();
  const QByteArray salt = QByteArray::fromBase64(root.value("salt").toString().toUtf8());
  const QByteArray nonce = QByteArray::fromBase64(root.value("nonce").toString().toUtf8());
  const QByteArray ciphertext = QByteArray::fromBase64(root.value("ciphertext").toString().toUtf8());

  QByteArray key(crypto_secretbox_KEYBYTES, '\0');
  const QByteArray passBytes = passphrase.toUtf8();
  if (crypto_pwhash(reinterpret_cast<unsigned char *>(key.data()),
                    static_cast<unsigned long long>(key.size()),
                    passBytes.constData(),
                    static_cast<unsigned long long>(passBytes.size()),
                    reinterpret_cast<const unsigned char *>(salt.constData()),
                    crypto_pwhash_OPSLIMIT_MODERATE, crypto_pwhash_MEMLIMIT_MODERATE,
                    crypto_pwhash_ALG_DEFAULT) != 0) {
    if (error) *error = "password hashing failed";
    return false;
  }

  QByteArray plaintext(ciphertext.size() - crypto_secretbox_MACBYTES, '\0');
  if (crypto_secretbox_open_easy(reinterpret_cast<unsigned char *>(plaintext.data()),
                                 reinterpret_cast<const unsigned char *>(ciphertext.constData()),
                                 ciphertext.size(),
                                 reinterpret_cast<const unsigned char *>(nonce.constData()),
                                 reinterpret_cast<const unsigned char *>(key.constData())) != 0) {
    if (error) *error = "decryption failed";
    return false;
  }

  const auto arr = QJsonDocument::fromJson(plaintext).array();
  if (profiles) {
    *profiles = profilesFromJson(arr);
  }
  return true;
#else
  Q_UNUSED(profiles)
  Q_UNUSED(passphrase)
  if (error) *error = "libsodium not available at build time";
  return false;
#endif
}
