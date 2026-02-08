#include "SshSession.h"

#include <QTimer>

#ifdef HAVE_LIBSSH
#include <libssh/libssh.h>
#endif

struct SshSession::Impl {
#ifdef HAVE_LIBSSH
  ssh_session session = nullptr;
  ssh_channel channel = nullptr;
  QTimer pollTimer;
#endif
};

SshSession::SshSession(QObject *parent) : QObject(parent), impl_(new Impl()) {
#ifdef HAVE_LIBSSH
  impl_->pollTimer.setInterval(30);
  connect(&impl_->pollTimer, &QTimer::timeout, this, [this]() {
    if (!impl_->channel) {
      return;
    }
    char buffer[4096];
    int n = ssh_channel_read_nonblocking(impl_->channel, buffer, sizeof(buffer), 0);
    if (n > 0) {
      emit output(QByteArray(buffer, n));
    }
    if (ssh_channel_is_eof(impl_->channel) || ssh_channel_is_closed(impl_->channel)) {
      disconnectFromHost();
    }
  });
#endif
}

SshSession::~SshSession() {
  disconnectFromHost();
  delete impl_;
}

void SshSession::connectToHost(const QString &host,
                               const QString &user,
                               const QString &password,
                               const QString &keyPath,
                               const QString &keyPassphrase,
                               int port) {
#ifdef HAVE_LIBSSH
  if (impl_->session) {
    disconnectFromHost();
  }

  impl_->session = ssh_new();
  if (!impl_->session) {
    emit error("Failed to create SSH session");
    return;
  }

  ssh_options_set(impl_->session, SSH_OPTIONS_HOST, host.toUtf8().constData());
  ssh_options_set(impl_->session, SSH_OPTIONS_USER, user.toUtf8().constData());
  ssh_options_set(impl_->session, SSH_OPTIONS_PORT, &port);

  int rc = ssh_connect(impl_->session);
  if (rc != SSH_OK) {
    emit error(QString("SSH connect failed: %1").arg(ssh_get_error(impl_->session)));
    disconnectFromHost();
    return;
  }

  if (!keyPath.isEmpty()) {
    ssh_key key = nullptr;
    const QByteArray keyPathBytes = keyPath.toUtf8();
    const QByteArray passBytes = keyPassphrase.toUtf8();
    const char *pass = passBytes.isEmpty() ? nullptr : passBytes.constData();
    rc = ssh_pki_import_privkey_file(keyPathBytes.constData(), pass, nullptr, nullptr, &key);
    if (rc != SSH_OK || !key) {
      emit error(QString("Failed to load key: %1").arg(ssh_get_error(impl_->session)));
      disconnectFromHost();
      return;
    }
    rc = ssh_userauth_publickey(impl_->session, nullptr, key);
    ssh_key_free(key);
  } else if (password.isEmpty()) {
    rc = ssh_userauth_publickey_auto(impl_->session, nullptr, nullptr);
  } else {
    rc = ssh_userauth_password(impl_->session, nullptr, password.toUtf8().constData());
  }

  if (rc != SSH_AUTH_SUCCESS) {
    emit error(QString("SSH auth failed: %1").arg(ssh_get_error(impl_->session)));
    disconnectFromHost();
    return;
  }

  impl_->channel = ssh_channel_new(impl_->session);
  if (!impl_->channel) {
    emit error("Failed to create SSH channel");
    disconnectFromHost();
    return;
  }

  rc = ssh_channel_open_session(impl_->channel);
  if (rc != SSH_OK) {
    emit error(QString("Failed to open channel: %1").arg(ssh_get_error(impl_->session)));
    disconnectFromHost();
    return;
  }

  rc = ssh_channel_request_pty(impl_->channel);
  if (rc != SSH_OK) {
    emit error(QString("Failed to request PTY: %1").arg(ssh_get_error(impl_->session)));
    disconnectFromHost();
    return;
  }

  rc = ssh_channel_request_shell(impl_->channel);
  if (rc != SSH_OK) {
    emit error(QString("Failed to request shell: %1").arg(ssh_get_error(impl_->session)));
    disconnectFromHost();
    return;
  }

  impl_->pollTimer.start();
  connected_ = true;
  emit connected();
#else
  Q_UNUSED(host)
  Q_UNUSED(user)
  Q_UNUSED(password)
  Q_UNUSED(keyPath)
  Q_UNUSED(keyPassphrase)
  Q_UNUSED(port)
  emit error("libssh not available at build time");
#endif
}

void SshSession::send(const QByteArray &data) {
#ifdef HAVE_LIBSSH
  if (!impl_->channel) {
    emit error("No active SSH channel");
    return;
  }
  ssh_channel_write(impl_->channel, data.constData(), data.size());
#else
  Q_UNUSED(data)
  emit error("libssh not available at build time");
#endif
}

void SshSession::disconnectFromHost() {
#ifdef HAVE_LIBSSH
  impl_->pollTimer.stop();
  if (impl_->channel) {
    ssh_channel_close(impl_->channel);
    ssh_channel_free(impl_->channel);
    impl_->channel = nullptr;
  }
  if (impl_->session) {
    ssh_disconnect(impl_->session);
    ssh_free(impl_->session);
    impl_->session = nullptr;
  }
  if (connected_) {
    connected_ = false;
    emit disconnected();
  }
#endif
}

void SshSession::setPtySize(int rows, int cols) {
#ifdef HAVE_LIBSSH
  if (!impl_->channel) {
    return;
  }
  ssh_channel_change_pty_size(impl_->channel, cols, rows);
#else
  Q_UNUSED(rows)
  Q_UNUSED(cols)
#endif
}
