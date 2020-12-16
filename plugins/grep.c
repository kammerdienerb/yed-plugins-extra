#include <yed/plugin.h>

void grep(int n_args, char **args);
void grep_start(void);
void grep_cleanup(void);
void grep_take_key(int key);
void grep_run(void);
void grep_select(void);
void grep_set_prompt(char *p, char *attr);

void grep_key_pressed_handler(yed_event *event);

yed_buffer *get_or_make_buff(void) {
    yed_buffer *buff;

    buff = yed_get_buffer("*grep-list");

    if (buff == NULL) {
        buff = yed_create_buffer("*grep-list");
        buff->flags |= BUFF_RD_ONLY | BUFF_SPECIAL;
    }

    return buff;
}

static char *prg;
static char  prompt_buff[256];
static char *save_current_search;

int yed_plugin_boot(yed_plugin *self) {
    yed_event_handler h;

    YED_PLUG_VERSION_CHECK();

    h.kind = EVENT_KEY_PRESSED;
    h.fn   = grep_key_pressed_handler;

    yed_plugin_add_event_handler(self, h);
    yed_plugin_set_command(self, "grep", grep);

    if (!yed_get_var("grep-prg")) {
        yed_set_var("grep-prg", "grep --exclude-dir={.git} -RHnIs '%' .");
    }

    return 0;
}

void grep(int n_args, char **args) {
    int key;

    if (!ys->interactive_command) {
        prg = yed_get_var("grep-prg");
        if (!prg) {
            yed_cerr("'grep-prg' not set");
            return;
        }
        grep_start();
    } else {
        sscanf(args[0], "%d", &key);
        grep_take_key(key);
    }
}

void grep_cleanup(void) {
    ys->save_search = save_current_search;
}

void grep_start(void) {
    ys->interactive_command = "grep";
    grep_set_prompt("(grep) ", NULL);
    save_current_search = ys->current_search;

    yed_buff_clear_no_undo(get_or_make_buff());
    YEXE("special-buffer-prepare-focus", "*grep-list");
    yed_frame_set_buff(ys->active_frame, get_or_make_buff());
    yed_set_cursor_far_within_frame(ys->active_frame, 1, 1);
    yed_clear_cmd_buff();
}

void grep_take_key(int key) {
    if (key == CTRL_C) {
        ys->interactive_command = NULL;
        ys->current_search      = NULL;
        yed_clear_cmd_buff();
        YEXE("special-buffer-prepare-unfocus", "*grep-list");
        grep_cleanup();
    } else if (key == ENTER) {
        ys->interactive_command = NULL;
        ys->current_search      = NULL;
        ys->active_frame->dirty = 1;
        yed_clear_cmd_buff();
    } else if (key == TAB) {
        grep_run();
    } else {
        if (key == BACKSPACE) {
            if (array_len(ys->cmd_buff)) {
                yed_cmd_buff_pop();
            }
        } else if (!iscntrl(key)) {
            yed_cmd_buff_push(key);
        }

        grep_run();
    }
}

void grep_run(void) {
    char       cmd_buff[1024];
    yed_attrs  attr_cmd, attr_attn;
    char       attr_buff[128];
    char      *pattern;
    int        len, status;

    grep_set_prompt("(grep) ", NULL);

    array_zero_term(ys->cmd_buff);

    cmd_buff[0]        = 0;
    pattern            = array_data(ys->cmd_buff);
    ys->current_search = pattern;

    if (strlen(pattern) == 0)     { goto empty; }

    len = perc_subst(prg, pattern, cmd_buff, sizeof(cmd_buff));

    ASSERT(len > 0, "buff too small for perc_subst");

    strcat(cmd_buff, " 2>/dev/null");

    if (yed_read_subproc_into_buffer(cmd_buff, get_or_make_buff(), &status) != 0) {
        goto err;
    }

    if (status != 0) {
err:;
        attr_cmd    = yed_active_style_get_command_line();
        attr_attn   = yed_active_style_get_attention();
        attr_cmd.fg = attr_attn.fg;
        yed_get_attr_str(attr_cmd, attr_buff);

        grep_set_prompt("(grep) ", attr_buff);
empty:;
        yed_buff_clear_no_undo(get_or_make_buff());
    }
}

void grep_select(void) {
    yed_line *line;
    char      _path[256];
    char     *path,
             *c;
    int       row,
              row_idx;

    path = _path;
    line = yed_buff_get_line(get_or_make_buff(), ys->active_frame->cursor_line);
    array_zero_term(line->chars);

    row_idx = 0;
    array_traverse(line->chars, c) {
        row_idx += 1;
        if (*c == ':') {
            break;
        }
        *path++ = *c;
    }
    *path = 0;
    path  = _path;

    if (row_idx >= 2) {
        if (*path == '.' && *(path + 1) == '/') {
            path += 2;
        }
    }

    sscanf(array_data(line->chars) + row_idx, "%d", &row);

    YEXE("special-buffer-prepare-jump-focus", "*grep-list");
    YEXE("buffer", path);
    yed_set_cursor_within_frame(ys->active_frame, 1, row);
    grep_cleanup();
}

void grep_key_pressed_handler(yed_event *event) {
    yed_frame *eframe;

    eframe = ys->active_frame;

    if (event->key != ENTER                           /* not the key we want */
    ||  ys->interactive_command                       /* still typing        */
    ||  !eframe                                       /* no frame            */
    ||  !eframe->buffer                               /* no buffer           */
    ||  strcmp(eframe->buffer->name, "*grep-list")) { /* not our buffer      */
        return;
    }

    grep_select();

    event->cancel = 1;
}

void grep_set_prompt(char *p, char *attr) {
    prompt_buff[0] = 0;

    strcat(prompt_buff, p);

    if (attr) {
        strcat(prompt_buff, attr);
    }

    ys->cmd_prompt = prompt_buff;
}
