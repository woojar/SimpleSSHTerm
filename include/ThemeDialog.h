#pragma once

#include <QDialog>
#include <QColor>
#include <QFont>

class QPushButton;
class QLabel;

class ThemeDialog : public QDialog {
  Q_OBJECT
public:
  explicit ThemeDialog(const QColor &fg, const QColor &bg, const QFont &font, QWidget *parent = nullptr);

  QColor foreground() const;
  QColor background() const;
  QFont font() const;

private slots:
  void chooseForeground();
  void chooseBackground();
  void chooseFont();
  void importBase16();
  void restoreDefaults();

private:
  void updatePreview();
  bool parseBase16(const QString &path, QColor *fg, QColor *bg, QString *error);

  QColor fg_;
  QColor bg_;
  QFont font_;

  QPushButton *fgButton_;
  QPushButton *bgButton_;
  QPushButton *fontButton_;
  QPushButton *defaultsButton_;
  QLabel *preview_;
};
