#pragma once

#include <QDialog>
#include <QVector>

#include "ProfileStore.h"

class QListWidget;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QToolButton;
class QCheckBox;

class ProfileManagerDialog : public QDialog {
  Q_OBJECT
public:
  explicit ProfileManagerDialog(QWidget *parent = nullptr);

  const Profile &selectedProfile() const;

private slots:
  void onSelectionChanged();
  void onAddProfile();
  void onSaveProfile();
  void onDeleteProfile();
  void onConnect();
  void onImportSshConfig();

private:
  bool loadProfiles();
  bool saveProfiles();
  bool promptPassphrase(bool confirmIfNew);
  void refreshList();
  void setFieldsFromProfile(const Profile &p);
  Profile profileFromFields() const;
  int currentIndex() const;
  QString storePath() const;

private:
  QVector<Profile> profiles_;
  Profile selected_;
  QString passphrase_;

  QListWidget *list_;
  QLineEdit *name_;
  QLineEdit *host_;
  QLineEdit *user_;
  QSpinBox *port_;
  QLineEdit *keyPath_;
  QToolButton *browseKeyButton_;
  QPushButton *addButton_;
  QPushButton *saveButton_;
  QPushButton *deleteButton_;
  QPushButton *importButton_;
  QPushButton *connectButton_;
  QCheckBox *protectCheck_;
  QCheckBox *openInNewTabCheck_;
};
