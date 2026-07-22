#include "ui.h"
#include "ui/input.h"
#include "ui/renderer.h"
#include "ui/style.h"
#include <string.h>
#include <termios.h>
#include <unistd.h>

int ui_init(Ui *ui) {
  *ui = (Ui){0};
  tcgetattr(STDIN_FILENO, &ui->tio);
  struct notcurses_options ncopts = {.flags = NCOPTION_SUPPRESS_BANNERS |
                                              NCOPTION_NO_FONT_CHANGES |
                                              NCOPTION_PRESERVE_CURSOR};
  ui->nc = notcurses_init(&ncopts, NULL);
  if (!ui->nc)
    return -1;
  style_init();
  return layout_create(ui->nc, &ui->layout);
}

void ui_suspend(Ui *ui) {
  if (!ui || !ui->nc)
    return;
  notcurses_stop(ui->nc);
  ui->nc = NULL;
  ui->layout = (Layout){0};
}

void ui_free_no_restore(Ui *ui) {
  if (!ui)
    return;
  layout_destroy(&ui->layout);
  write(STDOUT_FILENO, "\033[?1049l\033[?25h", 15);
  tcsetattr(STDIN_FILENO, TCSANOW, &ui->tio);
}

int ui_render(Ui *ui, AppState *app) {
  struct ncplane *std = notcurses_stdplane(ui->nc);
  ncplane_erase(std);

  if (strcmp(app->current.nav.path, "/") == 0) {
    ncplane_erase(ui->layout.parent);
    ncplane_putstr_yx(ui->layout.parent, 0, 0, "/");
  } else {
    render_pane(ui->layout.parent, &app->parent, false);
  }

  render_pane(ui->layout.current, &app->current, true);
  preview_check_cached(&app->preview);
  render_preview(ui->nc, ui->layout.preview, &app->preview, false,
                 app->config->preview_height, (int)ui->layout.rows - 1);
  render_status(ui->layout.cmdline, app);
  render_cmdline(ui->layout.cmdline, app);

  return notcurses_render(ui->nc);
}

int ui_handle_input(Ui *ui, AppState *app) {
  return input_handle(ui->nc, &ui->layout, app);
}
