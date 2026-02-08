#include "ThemeDialog.h"

#include <QColorDialog>
#include <QFontDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QLabel>
#include <QFileDialog>
#include <QFile>
#include <QPushButton>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QRegularExpression>

static QColor defaultFg() { return QColor(220, 220, 220); }
static QColor defaultBg() { return QColor(0, 0, 0); }
static QFont defaultFont() {
  QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  f.setPointSize(12);
  return f;
}

ThemeDialog::ThemeDialog(const QColor &fg, const QColor &bg, const QFont &font, QWidget *parent)
    : QDialog(parent), fg_(fg), bg_(bg), font_(font) {
  setWindowTitle("Theme");
  resize(420, 220);

  fgButton_ = new QPushButton("Choose...", this);
  bgButton_ = new QPushButton("Choose...", this);
  fontButton_ = new QPushButton("Choose...", this);
  auto *importButton = new QPushButton("Import Base16...", this);
  defaultsButton_ = new QPushButton("Restore Defaults", this);

  connect(fgButton_, &QPushButton::clicked, this, &ThemeDialog::chooseForeground);
  connect(bgButton_, &QPushButton::clicked, this, &ThemeDialog::chooseBackground);
  connect(fontButton_, &QPushButton::clicked, this, &ThemeDialog::chooseFont);
  connect(importButton, &QPushButton::clicked, this, &ThemeDialog::importBase16);
  connect(defaultsButton_, &QPushButton::clicked, this, &ThemeDialog::restoreDefaults);

  preview_ = new QLabel("Preview: The quick brown fox jumps over the lazy dog", this);
  preview_->setAutoFillBackground(true);
  preview_->setMargin(8);

  auto *form = new QFormLayout();
  form->addRow("Foreground", fgButton_);
  form->addRow("Background", bgButton_);
  form->addRow("Font", fontButton_);

  auto *buttons = new QHBoxLayout();
  auto *ok = new QPushButton("OK", this);
  auto *cancel = new QPushButton("Cancel", this);
  connect(ok, &QPushButton::clicked, this, &ThemeDialog::accept);
  connect(cancel, &QPushButton::clicked, this, &ThemeDialog::reject);
  buttons->addWidget(importButton);
  buttons->addWidget(defaultsButton_);
  buttons->addStretch(1);
  buttons->addWidget(ok);
  buttons->addWidget(cancel);

  auto *layout = new QVBoxLayout();
  layout->addLayout(form);
  layout->addWidget(preview_);
  layout->addLayout(buttons);
  setLayout(layout);

  updatePreview();
}

QColor ThemeDialog::foreground() const { return fg_; }
QColor ThemeDialog::background() const { return bg_; }
QFont ThemeDialog::font() const { return font_; }

void ThemeDialog::chooseForeground() {
  const QColor c = QColorDialog::getColor(fg_, this, "Foreground");
  if (c.isValid()) {
    fg_ = c;
    updatePreview();
  }
}

void ThemeDialog::chooseBackground() {
  const QColor c = QColorDialog::getColor(bg_, this, "Background");
  if (c.isValid()) {
    bg_ = c;
    updatePreview();
  }
}

void ThemeDialog::chooseFont() {
  bool ok = false;
  const QFont f = QFontDialog::getFont(&ok, font_, this, "Font");
  if (ok) {
    font_ = f;
    updatePreview();
  }
}

void ThemeDialog::restoreDefaults() {
  fg_ = defaultFg();
  bg_ = defaultBg();
  font_ = defaultFont();
  updatePreview();
}

void ThemeDialog::importBase16() {
  const QString path = QFileDialog::getOpenFileName(
      this, "Import Base16 Theme", QString(),
      "Base16 files (*.yaml *.yml *.json *.txt);;All files (*)");
  if (path.isEmpty()) {
    return;
  }

  QColor fg, bg;
  QString error;
  if (!parseBase16(path, &fg, &bg, &error)) {
    QMessageBox::warning(this, "Import Base16", error);
    return;
  }

  fg_ = fg;
  bg_ = bg;
  updatePreview();
}

void ThemeDialog::updatePreview() {
  preview_->setFont(font_);
  QPalette pal = preview_->palette();
  pal.setColor(QPalette::WindowText, fg_);
  pal.setColor(QPalette::Window, bg_);
  preview_->setPalette(pal);
  fgButton_->setText(fg_.name(QColor::HexRgb));
  bgButton_->setText(bg_.name(QColor::HexRgb));
  fontButton_->setText(QString("%1 %2pt").arg(font_.family()).arg(font_.pointSize()));
}

bool ThemeDialog::parseBase16(const QString &path, QColor *fg, QColor *bg, QString *error) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (error) *error = "Failed to open file";
    return false;
  }
  const QString text = QString::fromUtf8(f.readAll());

  auto findHex = [&](const QString &key, QString *out) -> bool {
    // Match: base05: "d0d0d0" or "base05": "d0d0d0"
    const QString pattern = QString("(?i)%1\\s*[:=]\\s*[\\\"']?([0-9a-f]{6})")
                                .arg(QRegularExpression::escape(key));
    QRegularExpression re(pattern);
    auto m = re.match(text);
    if (m.hasMatch()) {
      *out = m.captured(1);
      return true;
    }
    return false;
  };

  QString fgHex, bgHex;
  if (!findHex("base05", &fgHex)) {
    if (error) *error = "Base16 file missing base05 (foreground)";
    return false;
  }
  if (!findHex("base00", &bgHex)) {
    if (error) *error = "Base16 file missing base00 (background)";
    return false;
  }

  *fg = QColor("#" + fgHex);
  *bg = QColor("#" + bgHex);
  if (!fg->isValid() || !bg->isValid()) {
    if (error) *error = "Invalid Base16 colors";
    return false;
  }
  return true;
}
