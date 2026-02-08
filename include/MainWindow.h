#pragma once

#include <QMainWindow>
#include <QColor>
#include <QFont>
#include "ProfileStore.h"

class QTabWidget;
class QCloseEvent;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void newTab();
  void closeTab(int index);
  void onProfileConnected(const Profile &p);
  void onProfileSelected(const Profile &p);
  void onConnectInNewTab(const Profile &p);

private:
  bool restoreSessions();
  void openTabWithProfile(const Profile &p, bool autoConnect);
  QStringList loadLastSessions() const;
  void saveLastSessions(const QStringList &names) const;
  void loadTheme();
  void saveTheme() const;
  void applyThemeToAll();

  QTabWidget *tabs_;
  QColor themeFg_;
  QColor themeBg_;
  QFont themeFont_;
  bool closing_ = false;
};
