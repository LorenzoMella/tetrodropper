#include "tetrodropper.h"

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ncurses.h>
#include <time.h>



/*
 * General Game Settings
 */

void initialize(void)
{
  atexit(&cleanup);

#ifdef NDEBUG
  srand(time(0));		/* No randomisation for testing */
#endif
    
  /* ncurses configuration */
  initscr();
  raw();
  noecho();
  keypad(stdscr, true);
  nodelay(stdscr, true);
  curs_set(0);

  if (has_colors()) {

    start_color();

    init_pair(0, COLOR_WHITE, COLOR_BLACK);
    init_pair(I_TYPE, COLOR_MAGENTA, COLOR_WHITE);
    init_pair(J_TYPE, COLOR_YELLOW, COLOR_WHITE);
    init_pair(L_TYPE, COLOR_GREEN, COLOR_WHITE);
    init_pair(S_TYPE, COLOR_CYAN, COLOR_WHITE);
    init_pair(Z_TYPE, COLOR_GREEN, COLOR_WHITE);
    init_pair(O_TYPE, COLOR_RED, COLOR_WHITE);
    init_pair(T_TYPE, COLOR_BLUE, COLOR_WHITE);
    init_pair(DEAD_TYPE, COLOR_WHITE, COLOR_BLACK);
  }
}


void cleanup(void)
{
  curs_set(1);
  endwin();
}



/* 
 * Tetrodropper Logic
 */



enum TetrominoType random_type(void)
{
  return 1 + rand() % MAX_TYPES;
}


struct Tetromino *new_tetromino(enum TetrominoType type, int spawn_y, int spawn_x, WINDOW *win)
{
  struct Tetromino *t = malloc(sizeof(*t));
  Die(t == NULL);

  /* Initialize with the required template */
  switch (type) {

  case I_TYPE: *t = I_TETROMINO_TEMPLATE; break;
  case J_TYPE: *t = J_TETROMINO_TEMPLATE; break;
  case L_TYPE: *t = L_TETROMINO_TEMPLATE; break;
  case S_TYPE: *t = S_TETROMINO_TEMPLATE; break;
  case Z_TYPE: *t = Z_TETROMINO_TEMPLATE; break;
  case O_TYPE: *t = O_TETROMINO_TEMPLATE; break;
  default:
  case T_TYPE: *t = T_TETROMINO_TEMPLATE; break;
  }

  /* Translate the tetromino to the spawning position */
  reposition_tetromino(t, spawn_y, spawn_x, NULL, win);
  
  /* Ready */
  return t;
}


struct GameBoard *new_gameboard(int height, int width)
{
  struct GameBoard *board = malloc(sizeof(*board));
  Die(board == NULL);

  board->is_filled = malloc(height * sizeof(*board->is_filled));
  Die(board->is_filled == NULL);

  /* Initialize every row with zeroes (false) */
  for (size_t i = 0; i < height; ++i) {
    board->is_filled[i] = calloc(width, sizeof(board->is_filled[0][0]));
    Die(board->is_filled[i] == NULL);
  }
  
  board->height = height;
  board->width = width;

  return board;
}


void free_gameboard(struct GameBoard *board)
{
  free(board->is_filled);
  free(board);
}


struct Point rotate_point_90(struct Point p, int origin_y, int origin_x, bool counter_clockwise)
{
  /* 
   * The formula is essentially a rotation matrix on centered coordinates:
   *   new_pos = center + rot_matrix * (old_pos - center)
   */
  struct Point new_p;

  int rotation_sign = 2 * (int)counter_clockwise - 1;
  
  new_p.y = origin_y + rotation_sign * (p.x - origin_x);
  new_p.x = origin_x - rotation_sign * (p.y - origin_y);

  return new_p;
}


void recompute_bounding_box(struct Tetromino *t)
{
  t->min_y = Min(Min(t->square[0].y, t->square[1].y), Min(t->square[2].y, t->square[3].y));
  t->max_y = Max(Max(t->square[0].y, t->square[1].y), Max(t->square[2].y, t->square[3].y));
  t->min_x = Min(Min(t->square[0].x, t->square[1].x), Min(t->square[2].x, t->square[3].x));
  t->max_x = Max(Max(t->square[0].x, t->square[1].x), Max(t->square[2].x, t->square[3].x));
}


enum CollisionType rotate_tetromino(struct Tetromino *t, struct GameBoard *board, WINDOW *win)
{
  /* The 'O' tetromino doesn't rotate */
  if (t->num_states == 1) return NO_COLLISION;

  /* Rotation is clockwise only for already rotated 2-state tetrominoes */
  int counter_clockwise = t->num_states != 2 || t->rotation_state != 1;

  struct Tetromino new_t = *t;	/* Verify collisions non-destructively */
  
  for (int i = 0; i < 4; ++i) {
    new_t.square[i] = rotate_point_90(t->square[i], t->center_y, t->center_x, counter_clockwise);
  }

  enum CollisionType c = check_collision(&new_t, board);

  if (c == NO_COLLISION) {
    
    if (win != NULL) delete_tetromino(win, t, 0, 0);
    
    /* Update state, extremal values copy on the original data structure */
    new_t.rotation_state = (new_t.rotation_state + 1) % new_t.num_states;

    recompute_bounding_box(&new_t);

    *t = new_t;

    if (win != NULL) draw_tetromino(win, t, 0, 0);
  }
  
  return c;
}



enum CollisionType point_collision(struct Point p, struct GameBoard *board)
{
  assert(p.y >= 0);  /* The initial positioning should prevent this */
  
  if (p.x < 0 || p.x >= board->width) {
    
    return WALL_COLLISION;

  } else if (p.y >= board->height) {

    return FLOOR_COLLISION;

  } else if (board->is_filled[p.y][p.x]) { /* The test order guarantees indices not OOB */

    return DEAD_BLOCK_COLLISION;
    
  } else {

    return NO_COLLISION;
  }
}


enum CollisionType check_collision(struct Tetromino *t, struct GameBoard *board)
{
  for (int i = 0; i < 4; ++i) {

    enum CollisionType c = point_collision(t->square[i], board);

    if (c != NO_COLLISION) return c;
  }
  
  return NO_COLLISION;
}


enum CollisionType move_tetromino(struct Tetromino *t, struct GameBoard *board, int dy, int dx,
				  WINDOW *win)
{
  /* New tentative tetromino */
  struct Tetromino new_t = *t;

  /* Move it */
  for (int i = 0; i < 4; ++i) {
    new_t.square[i].y += dy;
    new_t.square[i].x += dx;
  }

  new_t.center_y += dy;
  new_t.center_x += dx;
    
  recompute_bounding_box(&new_t); /* Complete the structure */

  /* Verify that the new position doesn't result in collisions */
  enum CollisionType coll = check_collision(&new_t, board);

  if (coll == NO_COLLISION) {
    if (win != NULL) delete_tetromino(win, t, 0, 0);
    *t = new_t;
    if (win != NULL) draw_tetromino(win, t, 0, 0);
  }

  return coll;
}



void reposition_tetromino(struct Tetromino *t, int y, int x, WINDOW *from, WINDOW *to)
{
  if (from != NULL) delete_tetromino(from, t, 0, 0);

  for (int i = 0; i < 4; ++i) {
    t->square[i].y += y - t->center_y;
    t->square[i].x += x - t->center_x;
  }

  t->center_y = y;
  t->center_x = x;
    
  recompute_bounding_box(t);

  if (to != NULL) draw_tetromino(to, t, 0, 0);
}



void record_dead_blocks(struct Tetromino *t, struct GameBoard *board)
{
  for (int i = 0; i < 4; ++i) {
    board->is_filled[t->square[i].y][t->square[i].x] = true;
  }
}



bool row_is_full(struct GameBoard *board, int row)
{
  for (int j = 0; j < board->width; ++j) {
    if (!board->is_filled[row][j]) return false;
  }
  return true;
}



int remove_and_count_full_rows(struct GameBoard *board, int bottom_row, int top_row, WINDOW *win)
{
  int deleted = 0;
  int row = bottom_row;
  
  for (int i = 0; i < bottom_row - top_row + 1; ++i) {
    
    if (row_is_full(board, row)) {

      /* Delete the row in the board representation */
      free(board->is_filled[row]);
      /* 'Drop' all other rows (shift pointers down) */
      memmove(board->is_filled + 1, board->is_filled, row * sizeof(*board->is_filled));
      /* Replace the 'dropped' top row with a new empty one */
      board->is_filled[0] = calloc(board->width, sizeof(board->is_filled[0][0]));

      /* Visualize the effect on screen */
      if (win) draw_block_drop(win, row);
      
      deleted += 1;
      
    } else {
      /*
       * If the raw has been deleted, the blocks have dropped and 
       * we need to check the same row index again.
       * Otherwise, we check the row on top of that.
       */
      row -= 1;
    }
  }

  return deleted;
}



long score_from_lines(int num_lines)
{
  
  return 100 * ((1 << num_lines) / 2 + ((num_lines == 4) << 2));
}




/* 
 * Graphics
 */


void draw_tetromino(WINDOW *win, struct Tetromino * const t, int offset_y, int offset_x)
{
  wcolor_set(win, t->type, NULL);
  for (int i = 0; i < 4; ++i) { 
    mvwaddch(win, offset_y + t->square[i].y, offset_x + t->square[i].x, ACS_DIAMOND);
  }  
}


void delete_tetromino(WINDOW *win, struct Tetromino * const t, int offset_y, int offset_x)
{
  wcolor_set(win, 0, NULL);
  for (int i = 0; i < 4; ++i) {
    mvwaddch(win, offset_y + t->square[i].y, offset_x + t->square[i].x, ' ');
  }
}

void draw_board(WINDOW *win, int top_left_y, int top_left_x, int height, int width)
{
  /* Basic line */
  mvwvline(win, top_left_y, top_left_x - 1, ACS_VLINE, height);
  mvwvline(win, top_left_y, top_left_x + width, ACS_VLINE, height);
  mvwhline(win, top_left_y + height, top_left_x, ACS_HLINE, width);
  mvwaddch(win, top_left_y + height, top_left_x - 1, ACS_LLCORNER);
  mvwaddch(win, top_left_y + height, top_left_x + width, ACS_LRCORNER);
  
  /* Walls */
  mvwvline(win, top_left_y, top_left_x - 2, ACS_CKBOARD, height + 1);
  mvwvline(win, top_left_y, top_left_x + width + 1, ACS_CKBOARD, height + 1);
  mvwhline(win, top_left_y + height + 1, top_left_x - 2, ACS_CKBOARD, width + 4);
}


void draw_block_drop(WINDOW *win, int row)
{
  wmove(win, row, 0);
  wdeleteln(win);
  wmove(win, 0, 0);
  winsertln(win);
}


void draw_updated_stats(WINDOW *win, long score, double speed)
{
  int height, width;
  getmaxyx(win, height, width);
  
  char score_str[11];
  snprintf(score_str, 11, "%010ld", score);

  mvwaddstr(win, height / 3 - 1, (width - strlen(score_str)) / 2, "SCORE:");
  mvwaddstr(win, height / 3, (width - strlen(score_str)) / 2, score_str);

  char speed_str[11];
  snprintf(speed_str, 11, "%.1gx", speed);

  mvwaddstr(win, 2 * height / 3 - 1, (width - strlen(score_str)) / 2, "SPEED:");
  mvwaddstr(win, 2 * height / 3, (width - strlen(score_str)) / 2, speed_str);
  
}


/* 
 * Game Phases
 */


enum GameState title_screen(void)
{
  clear();
  
  box(stdscr, ACS_VLINE, ACS_HLINE);
  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  box(stdscr, ACS_VLINE, ACS_HLINE);

  char *welcome_string = "TETRODROPPER - Press [RET] to play or [Q] to quit.";
  mvprintw(screen_height / 2, (screen_width - strlen(welcome_string)) / 2, welcome_string);  
  
  refresh();

  while (true) {
    
    chtype ch = getch();

    if (ch == Ctrl('J')) return STATE_GAME;
    else if (ch == 's') return STATE_SCORES;
    else if (ch == 'q') return STATE_QUIT;
  }
}



double get_real_time(void)
{
  struct timespec tic;
  clock_gettime(CLOCK_REALTIME, &tic);

  /* Time in seconds, with fractional part up to (at most) nanoseconds */
  return tic.tv_sec + 1e-9 * tic.tv_nsec;
}



enum GameState game_screen(void)
{
  /* Prepare the game board */
  struct GameBoard *board = new_gameboard(BOARD_HEIGHT, BOARD_WIDTH);

  /* Create the game window hierarchy */
  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  int field_height = screen_height;
  int field_width = 2 * screen_width / 3;
  
  WINDOW *field_win = newwin(field_height, field_width, 0, 0);

  WINDOW *stats_win = newwin(screen_height, screen_width - field_width, 0, field_width);
  
  int board_origin_y = (field_height - board->height) / 2;
  int board_origin_x = (field_width - board->width) / 2;

  WINDOW *board_win = newwin(BOARD_HEIGHT, BOARD_WIDTH, board_origin_y, board_origin_x);

  WINDOW *preview_win = newwin(PREVIEW_WIN_SIDE, PREVIEW_WIN_SIDE,
			       board_origin_y,
			       board_origin_x + board->width + 4);
  
  /* Add the basic graphical decorations */
  draw_board(field_win, board_origin_y, board_origin_x, board->height, board->width);
  box(field_win, ACS_VLINE, ACS_HLINE);
  box(stats_win, ACS_VLINE, ACS_HLINE);
  box(preview_win, ACS_VLINE, ACS_HLINE);

  /* Game loop */

  double speed = 2.0;
  double dT = 1. / speed;  
  long score = 0;
  
  double threshold = dT + get_real_time();
  
  struct Tetromino *current_piece = new_tetromino(random_type(), 1, board->width / 2, board_win);
  struct Tetromino *preview_piece = new_tetromino(random_type(), PREVIEW_WIN_SIDE / 2,
						  PREVIEW_WIN_SIDE / 2, preview_win);
  
  bool gameover = false;

  while (!gameover) {

    draw_updated_stats(stats_win, score, speed);
    
    wnoutrefresh(field_win);
    wnoutrefresh(stats_win);
    wnoutrefresh(preview_win);
    wnoutrefresh(board_win);
    doupdate();

    /* Compute current time in seconds */

    /* Check for timed event */
    if (get_real_time() > threshold) {

      threshold += dT;

      enum CollisionType collision = move_tetromino(current_piece, board, +1, 0, board_win);

      if (collision != NO_COLLISION) {

	record_dead_blocks(current_piece, board);

	int num_deleted = remove_and_count_full_rows(board, current_piece->max_y,
						     current_piece->min_y, board_win);

	score += score_from_lines(num_deleted);
	
	free(current_piece);

	current_piece = preview_piece;

	reposition_tetromino(current_piece, 1, board->width / 2, preview_win, board_win);

	preview_piece = new_tetromino(random_type(), PREVIEW_WIN_SIDE / 2,
				      PREVIEW_WIN_SIDE / 2, preview_win);
      }
      
      continue;
    }

    /* Check for user event */
    chtype ch;
    if ((ch = getch()) != ERR) {

      if (ch == 'w' || ch == KEY_UP) {
	rotate_tetromino(current_piece, board, board_win);
      } else if (ch == 'a' || ch == KEY_LEFT) {
	move_tetromino(current_piece, board, 0, -1, board_win);
      } else if (ch == 's' || ch == KEY_DOWN) {
	move_tetromino(current_piece, board, +1, 0, board_win);
      } else if (ch == 'd' || ch == KEY_RIGHT) {
	move_tetromino(current_piece, board, 0, +1, board_win);
      } else {
        gameover = ch == Ctrl('Q'); /* Forced quit */
      }
    }
  }

  /* Cleanup */
  free(current_piece);
  free(preview_piece);
  free_gameboard(board);

  delwin(board_win);
  delwin(preview_win);
  delwin(stats_win);
  delwin(field_win);
  
  return STATE_QUIT;
}



int main(void)
{
  initialize();

  enum GameState next_state = STATE_TITLE;  

  while (true) {

    if (next_state == STATE_TITLE) {
      
      next_state = title_screen();
      
    } else if (next_state == STATE_GAME) {
      
      next_state = game_screen();
      
    } else if (next_state == STATE_SCORES) {
      
      break;			/* TEMPORARY */
      
    } else {
      
      break; 
    }
  }
}
