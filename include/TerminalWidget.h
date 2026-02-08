#pragma once

#include <QWidget>
#include <QColor>
#include <QFont>
#include <QByteArray>

#ifdef HAVE_LIBVTERM
#include <vterm.h>
#endif

class QTextEdit;
class QPlainTextEdit;

class TerminalWidget : public QWidget {
  Q_OBJECT
public:
  explicit TerminalWidget(QWidget *parent = nullptr);
  ~TerminalWidget() override;

  void writeData(const QByteArray &data);
  void clearScreen();
  void setTheme(const QColor &fg, const QColor &bg, const QFont &font);

signals:
  void sendData(const QByteArray &data);
  void terminalResized(int rows, int cols);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  bool handleKeyEvent(QKeyEvent *event);
  void pasteFromClipboard();
  void initFallbackUi();
#ifdef HAVE_LIBVTERM
  void initVTerm();
  void renderVTerm(QPainter &p);
  void updateSizeFromPixel();
  VTermPos pointToCell(const QPoint &p) const;
  QString selectedText() const;
#endif

private:
#ifdef HAVE_LIBVTERM
  VTerm *vterm_ = nullptr;
  VTermScreen *screen_ = nullptr;
  VTermState *state_ = nullptr;
  int cellWidth_ = 0;
  int cellHeight_ = 0;
  int cellAscent_ = 0;
  QFont font_;
  QColor fg_;
  QColor bg_;
  QTimer *cursorTimer_ = nullptr;
  QTimer *resizeTimer_ = nullptr;
  bool cursorVisible_ = true;
  bool vtermReady_ = false;
  int lastRows_ = 0;
  int lastCols_ = 0;
  VTermScreenCallbacks callbacks_ = {};
  bool selecting_ = false;
  VTermPos selStart_{0, 0};
  VTermPos selEnd_{0, 0};
  int cursorRow_ = 0;
  int cursorCol_ = 0;
  bool cursorShown_ = true;
  QPlainTextEdit *output_ = nullptr;
#else
  QPlainTextEdit *output_ = nullptr;
  QColor fg_;
  QColor bg_;
  QFont font_;
#endif
};
