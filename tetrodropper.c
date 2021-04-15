#include "tetrodropper.h"

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

  /* Screen preparation */
  wclear(stdscr);
}


void cleanup(void)
{
  endwin();
}



/* 
 * Tetrodropper Logic
 */


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
  reposition_tetromino(t, spawn_y, spawn_x, NULL, NULL);
  
  /* Ready */
  return t;

  if (win != NULL) draw_tetromino(win, t, 0, 0);
}


struct GameBoard *new_gameboard(int height, int width)
{
  struct GameBoard *board = malloc(sizeof(*board));
  Die(board == NULL);

  board->is_filled = malloc(height * sizeof(*board->is_filled));
  Die(board->is_filled == NULL);

  /* Initialize every row with zeroes */
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


struct Point rotate_point_90(struct Point p, int origin_y, int origin_x, int rotation_sign)
{
  /* 
   * The formula is essentially a rotation matrix on centered coordinates:
   *   new_pos = center + rot_matrix * (old_pos - center)
   */
  struct Point new_p;
  
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


void rotate_tetromino(struct Tetromino *t, struct GameBoard *board, WINDOW *win)
{
  if (win != NULL) delete_tetromino(win, t, 0, 0);
  
  /* The 'O' tetromino doesn't rotate */
  if (t->num_states == 1) return;

  struct Tetromino new_t = *t;	/* Test tetromino to verify collisions non-destructively */

  /* Rotation is clockwise only for already rotated 2-state tetrominoes */
  int rotation_sign = 2 * (t->num_states != 2 || t->rotation_state != 1) - 1;

  for (int i = 0; i < 4; ++i) {
    new_t.square[i] = rotate_point_90(t->square[i], t->center_y, t->center_x, rotation_sign);

    /* If any of the transformed points collide somewhere, the transformation is cancelled */
    if (point_collision(new_t.square[i], board) != NO_COLLISION) return;
  }

  /* No collision detected: update state, extremal values copy on the  original data structure */
  new_t.rotation_state = (new_t.rotation_state + 1) % new_t.num_states;

  recompute_bounding_box(t);
  
  *t = new_t;

  if (win != NULL) draw_tetromino(win, t, 0, 0);
}



enum CollisionType point_collision(struct Point p, struct GameBoard *board)
{
  if (p.x < 0 || p.x >= board->width) {
    
    return WALL_COLLISION;

  } else if (p.y >= board->height) {

    return FLOOR_COLLISION;

  } else if (board->is_filled[p.y][p.x]) { /* The order of tests guarantees indices not oob */

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

  /* Verify that the new position doesn't result in collisions */
  enum CollisionType coll = check_collision(&new_t, board);

  if (coll == NO_COLLISION) {
    
    if (win != NULL) delete_tetromino(win, t, 0, 0);
    
    recompute_bounding_box(&new_t); /* Complete the structure */
    *t = new_t;
    
    if (win != NULL) draw_tetromino(win, t, 0, 0);
  }

  return coll;
}



void reposition_tetromino(struct Tetromino *t, int y, int x, WINDOW *from, WINDOW *to)
{
  if (from != NULL) delete_tetromino(from, t, 0, 0);

  for (int i = 0; i < 4; ++i) {
    t->square[i].y += -t->center_y + y;
    t->square[i].x += -t->center_x + x;
  }

  t->center_y = y;
  t->center_x = x;
  
  recompute_bounding_box(t);

  if (from != NULL) draw_tetromino(to, t, 0, 0);
}



void dead_blocks_from_tetromino(struct Tetromino *t, struct GameBoard *board)
{
  for (int i = 0; i < 4; ++i) {
    board->is_filled[t->square[i].y][t->square[i].x] = true;
  }
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



/* 
 * Game States
 */


enum GameState title_screen(void)
{
  clear();
  
  box(stdscr, ACS_VLINE, ACS_HLINE);
  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  box(stdscr, ACS_VLINE, ACS_HLINE);

  char *welcome_string = "TETRODROPPER - Press ENTER to play.";
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

  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);
  
  WINDOW *field_win = newwin(screen_height, screen_width, 0, 0);
  box(field_win, ACS_VLINE, ACS_HLINE);

  
  int board_origin_y = (screen_height - board->height) / 2;
  int board_origin_x = (screen_width - board->width) / 2;

  WINDOW *board_win = newwin(BOARD_HEIGHT, BOARD_WIDTH, board_origin_y, board_origin_x);

  draw_board(field_win, board_origin_y, board_origin_x, board->height, board->width);

  wrefresh(board_win);
  wrefresh(field_win);

  /* Game loop */

  double speed = 2.0;
  double dT = 1. / speed;  
  int counter = 0;
  
  double threshold = dT + get_real_time();

  struct Tetromino *current_piece = new_tetromino(L_TYPE, 1, board->width / 2, board_win);
  struct Tetromino *preview_piece = new_tetromino(J_TYPE, 2, board->width / 2 + 10, board_win);
  
  bool gameover = false;

  while (!gameover) {

    wrefresh(board_win);
    wrefresh(field_win);

    
    /* Compute current time in seconds */
    double T = get_real_time();
    
    wrefresh(board_win);
    
    /* Check for timed event */
    if (T > threshold) {

      threshold += dT;

      enum CollisionType collision = move_tetromino(current_piece, board, +1, 0, board_win);

      if (collision != NO_COLLISION) {

	dead_blocks_from_tetromino(current_piece, board);

	free(current_piece);

	current_piece = preview_piece;

	reposition_tetromino(current_piece, 2, board->width / 2, board_win, board_win);

	preview_piece = new_tetromino(O_TYPE, 2, board->width / 2 + 10, board_win);
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
        gameover = ch == Ctrl('Q');
      }
    }
  }

  /* Cleanup */
  free(current_piece);
  free(preview_piece);
  free_gameboard(board);
  delwin(board_win);
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
