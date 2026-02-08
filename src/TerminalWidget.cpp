#include "TerminalWidget.h"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <cstring>
#include <QMimeData>
#include <QTimer>

#ifdef HAVE_LIBVTERM
#include <vterm.h>
#endif

TerminalWidget::TerminalWidget(QWidget *parent) : QWidget(parent) {
#ifdef HAVE_LIBVTERM
  font_ = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font_.setPointSize(12);
  fg_ = QColor(220, 220, 220);
  bg_ = QColor(0, 0, 0);

  // Use libvterm by default; allow disabling via env.
  bool useVterm = true;
  if (!qgetenv("SSH_TERMINAL_DISABLE_VTERM").isEmpty()) {
    useVterm = false;
  }

  if (useVterm) {
    initVTerm();
  } else {
    initFallbackUi();
  }
#else
  fg_ = QColor(220, 220, 220);
  bg_ = QColor(0, 0, 0);
  font_ = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font_.setPointSize(12);
  initFallbackUi();
#endif
  setTheme(fg_, bg_, font_);
}

TerminalWidget::~TerminalWidget() {
#ifdef HAVE_LIBVTERM
  if (vterm_) {
    vterm_free(vterm_);
    vterm_ = nullptr;
  }
#endif
}

void TerminalWidget::writeData(const QByteArray &data) {
#ifdef HAVE_LIBVTERM
  if (!vterm_) {
    return;
  }
  vterm_input_write(vterm_, data.constData(), data.size());
  vterm_screen_flush_damage(screen_);
  update();
#else
  if (!output_) {
    return;
  }
  auto stripAnsi = [](const QByteArray &in) -> QByteArray {
    QByteArray out;
    out.reserve(in.size());
    enum class State { Normal, Esc, Csi, Osc, OscEsc } state = State::Normal;
    for (char ch : in) {
      const unsigned char c = static_cast<unsigned char>(ch);
      switch (state) {
        case State::Normal:
          if (c == 0x1b) {
            state = State::Esc;
          } else {
            out.append(ch);
          }
          break;
        case State::Esc:
          if (c == '[') {
            state = State::Csi;
          } else if (c == ']') {
            state = State::Osc;
          } else {
            state = State::Normal;
          }
          break;
        case State::Csi:
          // CSI ends with @ through ~
          if (c >= 0x40 && c <= 0x7e) {
            state = State::Normal;
          }
          break;
        case State::Osc:
          if (c == 0x07) { // BEL
            state = State::Normal;
          } else if (c == 0x1b) {
            state = State::OscEsc;
          }
          break;
        case State::OscEsc:
          if (c == '\\') {
            state = State::Normal;
          } else if (c != 0x1b) {
            state = State::Osc;
          }
          break;
      }
    }
    return out;
  };

  const QByteArray clean = stripAnsi(data);
  output_->moveCursor(QTextCursor::End);
  output_->insertPlainText(QString::fromUtf8(clean));
  output_->moveCursor(QTextCursor::End);
#endif
}

void TerminalWidget::clearScreen() {
#ifdef HAVE_LIBVTERM
  // Avoid calling libvterm APIs here; some builds crash in screen flush/reset.
  // Clearing is handled by the remote terminal output itself.
  return;
#else
  if (output_) {
    output_->clear();
  }
#endif
}

void TerminalWidget::setTheme(const QColor &fg, const QColor &bg, const QFont &font) {
  fg_ = fg;
  bg_ = bg;
  font_ = font;

#if defined(__APPLE__)
  // Ensure emoji glyphs can render via fallback font on macOS.
  font_.setStyleHint(QFont::Monospace);
  font_.setFixedPitch(true);
  QStringList fams = font_.families();
  if (fams.isEmpty()) {
    fams.append(font_.family());
  }
  if (!fams.contains("Apple Color Emoji")) {
    fams.append("Apple Color Emoji");
  }
  font_.setFamilies(fams);
#endif

#ifdef HAVE_LIBVTERM
  QFontMetrics fm(font_);
  cellWidth_ = fm.horizontalAdvance(QLatin1Char('M'));
  cellHeight_ = fm.height();
  cellAscent_ = fm.ascent();
  if (vterm_ && isVisible()) {
    updateSizeFromPixel();
    update();
  } else if (vterm_) {
    QTimer::singleShot(0, this, [this]() {
      if (vterm_) {
        updateSizeFromPixel();
        update();
      }
    });
  }
#else
  if (output_) {
    output_->setFont(font_);
    QPalette pal = output_->palette();
    pal.setColor(QPalette::Base, bg_);
    pal.setColor(QPalette::Text, fg_);
    output_->setPalette(pal);
  }
#endif
}

bool TerminalWidget::handleKeyEvent(QKeyEvent *event) {
  QByteArray out;

  if ((event->modifiers() & Qt::ShiftModifier) &&
      (event->modifiers() & Qt::ControlModifier) &&
      event->key() == Qt::Key_V) {
    pasteFromClipboard();
    return true;
  }

  if ((event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_Insert) {
    pasteFromClipboard();
    return true;
  }

  if (event->modifiers() & Qt::ControlModifier) {
    const int key = event->key();
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
      char c = static_cast<char>(key - Qt::Key_A + 1);
      out.append(c);
    }
  } else {
    switch (event->key()) {
      case Qt::Key_Backspace:
        // Default to DEL (0x7f). Users can send ^H with Ctrl+H if needed.
        out.append("\x7f");
        break;
      case Qt::Key_Return:
      case Qt::Key_Enter:
        out.append("\r");
        break;
      case Qt::Key_Tab:
        out.append("\t");
        break;
      case Qt::Key_Escape:
        out.append("\x1b");
        break;
      case Qt::Key_Space:
        out.append(" ");
        break;
      case Qt::Key_Left:
        out.append("\x1b[D");
        break;
      case Qt::Key_Right:
        out.append("\x1b[C");
        break;
      case Qt::Key_Up:
        out.append("\x1b[A");
        break;
      case Qt::Key_Down:
        out.append("\x1b[B");
        break;
      case Qt::Key_Home:
        out.append("\x1b[H");
        break;
      case Qt::Key_End:
        out.append("\x1b[F");
        break;
      case Qt::Key_Delete:
        out.append("\x1b[3~");
        break;
      default:
        out.append(event->text().toUtf8());
        break;
    }
  }

  if (!out.isEmpty()) {
    emit sendData(out);
    return true;
  }
  return false;
}

void TerminalWidget::keyPressEvent(QKeyEvent *event) {
  if (handleKeyEvent(event)) {
    return;
  }
}

void TerminalWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
#ifdef HAVE_LIBVTERM
  QPainter p(this);
  p.fillRect(rect(), bg_.isValid() ? bg_ : Qt::black);
  renderVTerm(p);
#else
  QWidget::paintEvent(event);
#endif
}

void TerminalWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
#ifdef HAVE_LIBVTERM
  if (!resizeTimer_) {
    resizeTimer_ = new QTimer(this);
    resizeTimer_->setSingleShot(true);
    connect(resizeTimer_, &QTimer::timeout, this, [this]() {
      updateSizeFromPixel();
    });
  }
  resizeTimer_->start(0);
#endif
}

void TerminalWidget::focusInEvent(QFocusEvent *event) {
  QWidget::focusInEvent(event);
#ifdef HAVE_LIBVTERM
  cursorVisible_ = true;
  update();
#endif
}

void TerminalWidget::focusOutEvent(QFocusEvent *event) {
  QWidget::focusOutEvent(event);
#ifdef HAVE_LIBVTERM
  cursorVisible_ = false;
  update();
#endif
}

void TerminalWidget::mousePressEvent(QMouseEvent *event) {
#ifdef HAVE_LIBVTERM
  if (event->button() == Qt::LeftButton) {
    selecting_ = true;
    selStart_ = pointToCell(event->pos());
    selEnd_ = selStart_;
    update();
    return;
  }
  if (event->button() == Qt::MiddleButton) {
    pasteFromClipboard();
    return;
  }
#endif
  QWidget::mousePressEvent(event);
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *event) {
#ifdef HAVE_LIBVTERM
  if (selecting_) {
    selEnd_ = pointToCell(event->pos());
    update();
    return;
  }
#endif
  QWidget::mouseMoveEvent(event);
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *event) {
#ifdef HAVE_LIBVTERM
  if (event->button() == Qt::LeftButton && selecting_) {
    selecting_ = false;
    const QString text = selectedText();
    if (!text.isEmpty()) {
      QClipboard *cb = QApplication::clipboard();
      if (cb) {
        cb->setText(text, QClipboard::Clipboard);
        if (cb->supportsSelection()) {
          cb->setText(text, QClipboard::Selection);
        }
      }
    }
    update();
    return;
  }
#endif
  QWidget::mouseReleaseEvent(event);
}

void TerminalWidget::initFallbackUi() {
  class FallbackEdit : public QPlainTextEdit {
  public:
    explicit FallbackEdit(TerminalWidget *owner) : QPlainTextEdit(owner), owner_(owner) {}
  protected:
    void keyPressEvent(QKeyEvent *event) override {
      if (event->matches(QKeySequence::Copy)) {
        copy();
        return;
      }
      if (event->matches(QKeySequence::Paste)) {
        owner_->pasteFromClipboard();
        return;
      }
      if (owner_->handleKeyEvent(event)) {
        return;
      }
      QPlainTextEdit::keyPressEvent(event);
    }
    void insertFromMimeData(const QMimeData *source) override {
      if (owner_ && source) {
        owner_->sendData(source->text().toUtf8());
      }
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
      QPlainTextEdit::mouseReleaseEvent(event);
      if (textCursor().hasSelection()) {
        const QString selected = textCursor().selectedText();
        QClipboard *cb = QApplication::clipboard();
        if (cb) {
          cb->setText(selected, QClipboard::Clipboard);
          if (cb->supportsSelection()) {
            cb->setText(selected, QClipboard::Selection);
          }
        }
      }
    }
    void mousePressEvent(QMouseEvent *event) override {
      if (event->button() == Qt::MiddleButton) {
        QClipboard *cb = QApplication::clipboard();
        if (cb) {
          QString text = cb->text(QClipboard::Selection);
          if (text.isEmpty()) {
            text = cb->text(QClipboard::Clipboard);
          }
          if (!text.isEmpty() && owner_) {
            owner_->sendData(text.toUtf8());
            return;
          }
        }
      }
      QPlainTextEdit::mousePressEvent(event);
    }
  private:
    TerminalWidget *owner_;
  };

  auto *layout = new QVBoxLayout(this);
  output_ = new FallbackEdit(this);
  output_->setReadOnly(false);
  output_->setPlaceholderText("Terminal output will appear here.");
  output_->setUndoRedoEnabled(false);
  output_->setLineWrapMode(QPlainTextEdit::NoWrap);
  output_->document()->setMaximumBlockCount(5000);

  layout->addWidget(output_);
}

void TerminalWidget::pasteFromClipboard() {
  const QClipboard *cb = QApplication::clipboard();
  if (!cb) {
    return;
  }
  const QString text = cb->text();
  if (text.isEmpty()) {
    return;
  }
  emit sendData(text.toUtf8());
}

#ifdef HAVE_LIBVTERM
void TerminalWidget::initVTerm() {
  setFocusPolicy(Qt::StrongFocus);

  QFontMetrics fm(font_);
  cellWidth_ = fm.horizontalAdvance(QLatin1Char('M'));
  cellHeight_ = fm.height();
  cellAscent_ = fm.ascent();

  const int cols = qMax(100, width() / qMax(1, cellWidth_));
  const int rows = qMax(24, height() / qMax(1, cellHeight_));

  vterm_ = vterm_new(rows, cols);
  vterm_set_utf8(vterm_, 1);
  screen_ = vterm_obtain_screen(vterm_);
  state_ = vterm_obtain_state(vterm_);
  vtermReady_ = (vterm_ != nullptr);
  lastRows_ = rows;
  lastCols_ = cols;

  // Initialize screen/state buffers to avoid crashes on first input.
  if (screen_) {
    vterm_screen_reset(screen_, 1);
  }
  if (state_) {
    vterm_state_reset(state_, 1);
  }

  callbacks_ = {};
  callbacks_.damage = [](VTermRect rect, void *user) -> int {
    auto *self = static_cast<TerminalWidget *>(user);
    Q_UNUSED(rect)
    self->update();
    return 1;
  };
  callbacks_.movecursor = [](VTermPos pos, VTermPos oldpos, int visible, void *user) -> int {
    Q_UNUSED(oldpos)
    auto *self = static_cast<TerminalWidget *>(user);
    self->cursorRow_ = pos.row;
    self->cursorCol_ = pos.col;
    self->cursorShown_ = (visible != 0);
    self->update();
    return 1;
  };
  callbacks_.sb_pushline = [](int cols, const VTermScreenCell *cells, void *user) -> int {
    Q_UNUSED(cols)
    Q_UNUSED(cells)
    Q_UNUSED(user)
    // No scrollback buffer; ignore.
    return 1;
  };
  callbacks_.sb_popline = [](int cols, VTermScreenCell *cells, void *user) -> int {
    Q_UNUSED(cols)
    Q_UNUSED(cells)
    Q_UNUSED(user)
    return 1;
  };
  callbacks_.sb_clear = [](void *user) -> int {
    Q_UNUSED(user)
    return 1;
  };

  vterm_screen_set_callbacks(screen_, &callbacks_, this);
  vterm_screen_set_damage_merge(screen_, VTERM_DAMAGE_SCROLL);

  // Apply default colors from theme to vterm state/screen
  if (screen_ && state_) {
    VTermColor fg;
    VTermColor bg;
    const QColor fgq = fg_.isValid() ? fg_ : QColor(220, 220, 220);
    const QColor bgq = bg_.isValid() ? bg_ : QColor(0, 0, 0);
    vterm_color_rgb(&fg, static_cast<uint8_t>(fgq.red()), static_cast<uint8_t>(fgq.green()), static_cast<uint8_t>(fgq.blue()));
    vterm_color_rgb(&bg, static_cast<uint8_t>(bgq.red()), static_cast<uint8_t>(bgq.green()), static_cast<uint8_t>(bgq.blue()));
    vterm_state_set_default_colors(state_, &fg, &bg);
    vterm_screen_set_default_colors(screen_, &fg, &bg);
  }

  if (!cursorTimer_) {
    cursorTimer_ = new QTimer(this);
    cursorTimer_->setInterval(600);
    connect(cursorTimer_, &QTimer::timeout, this, [this]() {
      cursorVisible_ = !cursorVisible_;
      update();
    });
    cursorTimer_->start();
  }

  emit terminalResized(rows, cols);
}

void TerminalWidget::updateSizeFromPixel() {
  if (!vterm_ || !vtermReady_) {
    return;
  }
  if (cellWidth_ <= 0 || cellHeight_ <= 0) {
    return;
  }
  const int w = width();
  const int h = height();
  if (w <= 0 || h <= 0) {
    return;
  }
  int cols = qMax(100, w / cellWidth_);
  int rows = qMax(24, h / cellHeight_);
  cols = qMin(cols, 1000);
  rows = qMin(rows, 1000);
  if (rows == lastRows_ && cols == lastCols_) {
    return;
  }
  lastRows_ = rows;
  lastCols_ = cols;
  // macOS libvterm builds have been unstable in resize_buffer; avoid resizing.
#if defined(__APPLE__)
  emit terminalResized(rows, cols);
  return;
#else
  vterm_set_size(vterm_, rows, cols);
  emit terminalResized(rows, cols);
  update();
#endif
}

VTermPos TerminalWidget::pointToCell(const QPoint &p) const {
  int row = 0;
  int col = 0;
  if (cellWidth_ > 0 && cellHeight_ > 0) {
    col = p.x() / cellWidth_;
    row = p.y() / cellHeight_;
  }
  int rows = 0;
  int cols = 0;
  if (vterm_) {
    vterm_get_size(vterm_, &rows, &cols);
  }
  if (rows > 0) {
    row = qBound(0, row, rows - 1);
  }
  if (cols > 0) {
    col = qBound(0, col, cols - 1);
  }
  return VTermPos{row, col};
}

QString TerminalWidget::selectedText() const {
  if (!screen_ || !vterm_) {
    return QString();
  }
  int rows = 0;
  int cols = 0;
  vterm_get_size(vterm_, &rows, &cols);
  if (rows <= 0 || cols <= 0) {
    return QString();
  }

  VTermPos a = selStart_;
  VTermPos b = selEnd_;
  if (b.row < a.row || (b.row == a.row && b.col < a.col)) {
    VTermPos tmp = a;
    a = b;
    b = tmp;
  }

  QStringList lines;
  for (int r = a.row; r <= b.row && r < rows; ++r) {
    int c0 = (r == a.row) ? a.col : 0;
    int c1 = (r == b.row) ? b.col : (cols - 1);
    if (c0 > c1) {
      std::swap(c0, c1);
    }
    QString line;
    line.reserve(c1 - c0 + 1);
    for (int c = c0; c <= c1 && c < cols; ++c) {
      VTermPos pos{r, c};
      VTermScreenCell cell;
      if (!vterm_screen_get_cell(screen_, pos, &cell)) {
        line.append(' ');
        continue;
      }
      uint32_t ch = cell.chars[0];
      if (ch == 0) {
        ch = ' ';
      }
      line.append(QString::fromUcs4(&ch, 1));
    }
    int end = line.length();
    while (end > 0 && line.at(end - 1) == QLatin1Char(' ')) {
      --end;
    }
    lines.append(line.left(end));
  }

  return lines.join("\n");
}

static QColor vtermColorToQColor(const VTermScreen *screen, VTermColor col, const QColor &fallback) {
  if (VTERM_COLOR_IS_DEFAULT_FG(&col) || VTERM_COLOR_IS_DEFAULT_BG(&col)) {
    return fallback;
  }
  if (VTERM_COLOR_IS_INDEXED(&col)) {
    VTermColor tmp = col;
    vterm_screen_convert_color_to_rgb(screen, &tmp);
    col = tmp;
  }
  if (VTERM_COLOR_IS_RGB(&col)) {
    return QColor(col.rgb.red, col.rgb.green, col.rgb.blue);
  }
  return fallback;
}

void TerminalWidget::renderVTerm(QPainter &p) {
  if (!screen_) {
    return;
  }
  if (cellWidth_ <= 0 || cellHeight_ <= 0) {
    return;
  }

  p.setFont(font_);

  int rows = 0;
  int cols = 0;
  vterm_get_size(vterm_, &rows, &cols);

  const QColor defaultFg = fg_.isValid() ? fg_ : QColor(220, 220, 220);
  const QColor defaultBg = bg_.isValid() ? bg_ : QColor(0, 0, 0);
  const QColor selBg(80, 120, 200);
  const QColor selFg(255, 255, 255);

  VTermPos a = selStart_;
  VTermPos b = selEnd_;
  bool hasSelection = (a.row != b.row) || (a.col != b.col) || selecting_;
  if (hasSelection && (b.row < a.row || (b.row == a.row && b.col < a.col))) {
    VTermPos tmp = a;
    a = b;
    b = tmp;
  }

  VTermScreenCell cell;

  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      VTermPos pos{r, c};
      if (!vterm_screen_get_cell(screen_, pos, &cell)) {
        continue;
      }

      QColor fg = vtermColorToQColor(screen_, cell.fg, defaultFg);
      QColor bg = vtermColorToQColor(screen_, cell.bg, defaultBg);
      if (cell.attrs.reverse) {
        QColor tmp = fg;
        fg = bg;
        bg = tmp;
      }
      if (hasSelection) {
        bool inSel = false;
        if (r > a.row && r < b.row) {
          inSel = true;
        } else if (r == a.row && r == b.row) {
          inSel = (c >= a.col && c <= b.col);
        } else if (r == a.row) {
          inSel = (c >= a.col);
        } else if (r == b.row) {
          inSel = (c <= b.col);
        }
        if (inSel) {
          bg = selBg;
          fg = selFg;
        }
      }

      const int x = c * cellWidth_;
      const int y = r * cellHeight_;

      p.fillRect(QRect(x, y, cellWidth_, cellHeight_), bg);

      uint32_t ch = cell.chars[0];
      if (ch == 0) {
        ch = ' ';
      }
      const QString s = QString::fromUcs4(&ch, 1);
      p.setPen(fg);
      p.drawText(x, y + cellAscent_, s);
    }
  }

  if (cursorVisible_) {
    VTermPos cpos{cursorRow_, cursorCol_};
    if (state_) {
      vterm_state_get_cursorpos(state_, &cpos);
    }
    if (cpos.row >= 0 && cpos.row < rows && cpos.col >= 0 && cpos.col < cols) {
      const int x = cpos.col * cellWidth_;
      const int y = cpos.row * cellHeight_;
      VTermScreenCell ccell;
      if (vterm_screen_get_cell(screen_, cpos, &ccell)) {
        QColor fg = vtermColorToQColor(screen_, ccell.fg, defaultFg);
        QColor bg = vtermColorToQColor(screen_, ccell.bg, defaultBg);
        // Invert colors for visibility
        p.fillRect(QRect(x, y, cellWidth_, cellHeight_), fg);
        uint32_t ch = ccell.chars[0];
        if (ch == 0) {
          ch = ' ';
        }
        const QString s = QString::fromUcs4(&ch, 1);
        p.setPen(bg);
        p.drawText(x, y + cellAscent_, s);
      } else {
        p.setPen(QPen(defaultFg));
        p.drawRect(QRect(x, y, cellWidth_ - 1, cellHeight_ - 1));
      }
    }
  }
}
#endif
