#include "tetrodropper.h"

#include <assert.h>
#include <ctype.h>
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



/*
 * Game Mechanics
 */


void recompute_bounding_box(struct Tetromino *t)
{
  t->min_y = Min(Min(t->square[0].y, t->square[1].y), Min(t->square[2].y, t->square[3].y));
  t->max_y = Max(Max(t->square[0].y, t->square[1].y), Max(t->square[2].y, t->square[3].y));
  t->min_x = Min(Min(t->square[0].x, t->square[1].x), Min(t->square[2].x, t->square[3].x));
  t->max_x = Max(Max(t->square[0].x, t->square[1].x), Max(t->square[2].x, t->square[3].x));
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
  for (int i = 0; i < MAX_BLOCKS; ++i) {

    enum CollisionType c = point_collision(t->square[i], board);

    if (c != NO_COLLISION) return c;
  }
  
  return NO_COLLISION;
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



enum CollisionType rotate_tetromino(struct Tetromino *t, struct GameBoard *board, WINDOW *win)
{
  /* The 'O' tetromino doesn't rotate */
  if (t->num_states == 1) return NO_COLLISION;

  /* Rotation is clockwise only for already rotated 2-state tetrominoes */
  int counter_clockwise = t->num_states != 2 || t->rotation_state != 1;

  struct Tetromino new_t = *t;	/* Verify collisions non-destructively */
  
  for (int i = 0; i < MAX_BLOCKS; ++i) {
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



enum CollisionType move_tetromino(struct Tetromino *t, struct GameBoard *board, int dy, int dx,
				  WINDOW *win)
{
  /* New tentative tetromino */
  struct Tetromino new_t = *t;

  /* Move it */
  for (int i = 0; i < MAX_BLOCKS; ++i) {
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

  for (int i = 0; i < MAX_BLOCKS; ++i) {
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
  for (int i = 0; i < MAX_BLOCKS; ++i) {
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
      if (win) animate_drop(win, row);
      
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



double speed_from_score(long score)
{
  return INITIAL_SPEED + (score / SCORE_MODULUS) * SPEED_INCREMENT;
}



double get_real_time(void)
{
  struct timespec tic;
  clock_gettime(CLOCK_REALTIME, &tic);

  /* Time in seconds, with fractional part up to (at most) nanoseconds */
  return tic.tv_sec + 1e-9 * tic.tv_nsec;
}





/* 
 * Graphics
 */


void draw_tetromino(WINDOW *win, struct Tetromino * const t, int offset_y, int offset_x)
{
  wcolor_set(win, t->type, NULL);
  for (int i = 0; i < MAX_BLOCKS; ++i) { 
    mvwaddch(win, offset_y + t->square[i].y, offset_x + t->square[i].x, ACS_DIAMOND);
  }  
}


void delete_tetromino(WINDOW *win, struct Tetromino * const t, int offset_y, int offset_x)
{
  wcolor_set(win, 0, NULL);
  for (int i = 0; i < MAX_BLOCKS; ++i) {
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


void animate_drop(WINDOW *win, int row)
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
  snprintf(speed_str, 11, "%.2gx", speed);

  mvwaddstr(win, 2 * height / 3 - 1, (width - strlen(score_str)) / 2, "SPEED:");
  mvwaddstr(win, 2 * height / 3, (width - strlen(score_str)) / 2, speed_str);
  
}


WINDOW *draw_message_popup(int col_offt, char *msg)
{
  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);
  
  WINDOW *win = newwin(5, strlen(msg) + 4,
		       (screen_height) / 2 - 2 + col_offt,
		       (screen_width - strlen(msg)) / 2);
  
  box(win, ACS_VLINE, ACS_HLINE);

  mvwaddnstr(win, 2, 2, msg, strlen(msg));
  
  return win;
}


/* 
 * Game Phases
 */


enum GameState title_screen(void)
{
  clear();
    
  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  for (int i = 0; i < TITLE_HEIGHT; ++i)
    mvprintw(screen_height / 3 + i, (screen_width - TITLE_WIDTH) / 2, title_string[i]);
  
  char *welcome_string = "Press [RET] to play, [S] to display the rankings, or [Q] to quit.";
  mvprintw(screen_height * 2 / 3, (screen_width - strlen(welcome_string)) / 2, welcome_string);

  box(stdscr, ACS_VLINE, ACS_HLINE);
    
  wrefresh(stdscr);

  enum GameState next_state;
  
  while (true) {
    chtype ch = getch();
    if (ch == KEY_RETURN) {
      next_state = STATE_GAME;
      break;
    } else if (toupper(ch) == 'S') {
      next_state = STATE_SCORES;
      break;
    } else if (toupper(ch) == 'Q') {
      next_state = STATE_QUIT;
      break;
    }
  }

  return next_state;
}


enum GameState score_screen(struct Ranking rankings[MAX_RANKINGS + 1])
{
  clear();
  
  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);
  
  char *score_str = "TOP-10 RANKINGS";
  mvprintw(screen_height / 4, (screen_width - strlen(score_str)) / 2, score_str);
  
  for (int i = 0; i < MAX_RANKINGS; ++i) {
    mvprintw(screen_height / 4 + 2 + i, (screen_width - 15) / 2, "%s  %010ld",
	     rankings[i].name, rankings[i].score);
  }

  char *msg_string = "Press [T] to go back to the Title Screen or [Q] to quit.";
  mvprintw(screen_height / 4 + 13, (screen_width - strlen(msg_string)) / 2, msg_string);

  
  box(stdscr, ACS_VLINE, ACS_HLINE);
    
  refresh();

  enum GameState next_state;
  
  while (true) {
    chtype ch = getch();
    if (toupper(ch) == 'T') {
      next_state = STATE_TITLE;
      break;
    } else if (toupper(ch) == 'Q') {
      next_state = STATE_QUIT;
      break;
    }
  }

  return next_state;
}


enum GameState manage_gameover(void)
{
  char msg[] = "Game Over. Press [T] to go to the title screen or [Q] to quit.";

  WINDOW *popup_win = draw_message_popup(0, msg);

  wrefresh(popup_win);
  
  enum GameState next_state;

  while (true) {
    chtype ch = getch();
    if (toupper(ch) == 'T') {
      next_state = STATE_TITLE;
      break;
    } else if (toupper(ch) == 'Q') {
      next_state = STATE_QUIT;
      break;
    }
  }
  
  delwin(popup_win);
  
  return next_state;
}


/* Comparison function for qsort */
bool ranking_ge(struct Ranking *a, struct Ranking *b)
{
  return a->score < b->score;
}


void insert_ranking_name(char *name)
{
  /* Display message and create box window to insert the initials */
  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  WINDOW *msg_win = draw_message_popup(-5, "You made it into the Top-10! "
				       "Insert your initials. Press [RET] to end.");
  
  WINDOW *insert_box = newwin(7, 11, (screen_height - 7) * 2 / 3, (screen_width - 11) / 2);

  box(insert_box, ACS_VLINE, ACS_HLINE);

  /* Add arrow graphics to suggest UI */
  mvwaddch(insert_box, 2, 4, ACS_UARROW);
  mvwaddch(insert_box, 2, 5, ACS_UARROW);
  mvwaddch(insert_box, 2, 6, ACS_UARROW);
  mvwaddch(insert_box, 3, 3, ACS_LARROW);
  mvwaddch(insert_box, 3, 7, ACS_RARROW);
  mvwaddch(insert_box, 4, 4, ACS_DARROW);
  mvwaddch(insert_box, 4, 5, ACS_DARROW);
  mvwaddch(insert_box, 4, 6, ACS_DARROW);
  
  /* Name insertion */
  char inserted_name[NAME_BUF_LEN] = "AAA";

  curs_set(1);		       /* Display native cursor for clarity */

  int i = 0;	      /* Position currently edited in inserted_name */

  wmove(insert_box, 3, 4);
  waddstr(insert_box, inserted_name);
  
  while (true) {
    wnoutrefresh(msg_win);
    wrefresh(insert_box);

    wmove(insert_box, 3, i + 4);
    
    chtype ch;

    if ((ch = getch()) != ERR) {

      if (ch == KEY_UP) {
	
	inserted_name[i] = NextChar(inserted_name[i]);
	waddch(insert_box, inserted_name[i]);
	
      } else if (ch == KEY_LEFT) {
	
	i = (i + (NAME_BUF_LEN - 1) - 1) % (NAME_BUF_LEN - 1);
	
      } else if (ch == KEY_DOWN) {
	
	inserted_name[i] = PrevChar(inserted_name[i]);
	waddch(insert_box, inserted_name[i]);
	
      } else if (ch == KEY_RIGHT) {
	
        i = (i + 1) % (NAME_BUF_LEN - 1);
	
      } else if (ch == KEY_RETURN) { /* Return Key */
	
        break;
      }
    }
  }
  
  curs_set(0);			/* Hide native cursor again */
  
  delwin(insert_box);
  delwin(msg_win);
  
  strncpy(name, inserted_name, 4);
}


bool top_score(struct Ranking rankings[MAX_RANKINGS + 1], long new_score)
{
  return new_score > rankings[MAX_RANKINGS - 1].score;
}


void record_ranking(struct Ranking rankings[MAX_RANKINGS + 1], char *new_name, long new_score)
{
  /* Add new ranking in the temporary slot */
  strncpy(rankings[MAX_RANKINGS].name, new_name, NAME_BUF_LEN);
  rankings[MAX_RANKINGS].score = new_score;

  /* Sort all the slots */
  qsort(rankings, MAX_RANKINGS + 1, sizeof(*rankings), ranking_ge);

  /* Reset the temporary slot */
  strncpy(rankings[MAX_RANKINGS].name, "???", NAME_BUF_LEN);
  rankings[MAX_RANKINGS].score = 0L;
}


enum GameState game_screen(struct Ranking rankings[MAX_RANKINGS + 1])
{
  /* Prepare the game board */
  struct GameBoard *board = new_gameboard(BOARD_HEIGHT, BOARD_WIDTH);

  /* Create the game window hierarchy */
  int screen_height, screen_width;
  getmaxyx(stdscr, screen_height, screen_width);

  int field_height = screen_height;
  int field_width = 2 * screen_width / 3;
  
  WINDOW *field_win = newwin(field_height, field_width, 0, 0);

  WINDOW *side_win = newwin(screen_height, screen_width - field_width, 0, field_width);
  
  int board_origin_y = (field_height - board->height) / 2;
  int board_origin_x = (field_width - board->width) / 2;

  WINDOW *board_win = newwin(BOARD_HEIGHT, BOARD_WIDTH, board_origin_y, board_origin_x);

  WINDOW *preview_win = newwin(PREVIEW_WIN_SIDE, PREVIEW_WIN_SIDE,
			       board_origin_y,
			       board_origin_x + board->width + 4);
  
  /* Add the basic graphical decorations */
  draw_board(field_win, board_origin_y, board_origin_x, board->height, board->width);
  box(field_win, ACS_VLINE, ACS_HLINE);
  box(side_win, ACS_VLINE, ACS_HLINE);
  box(preview_win, ACS_VLINE, ACS_HLINE);

  /* Game loop */
  
  struct Tetromino *current_piece = new_tetromino(random_type(), SPAWN_HEIGHT, SPAWN_WIDTH,
						  board_win);
  struct Tetromino *preview_piece = new_tetromino(random_type(), PREVIEW_WIN_SIDE / 2 - 1,
						  PREVIEW_WIN_SIDE / 2, preview_win);

  long score = 0;

  double threshold = 1. / INITIAL_SPEED + get_real_time();

  bool gameover = false;
  
  while (!gameover) {

    double speed = speed_from_score(score);

    draw_updated_stats(side_win, score, speed);

    /* Refresh all screen assets */
    wnoutrefresh(field_win);
    wnoutrefresh(side_win);
    wnoutrefresh(preview_win);
    wnoutrefresh(board_win);
    doupdate();

    /* Timed event management */
    if (get_real_time() >= threshold) {

      threshold += 1. / speed;
      
      enum CollisionType collision = move_tetromino(current_piece, board, +1, 0, board_win);

      if (collision != NO_COLLISION) {

	/* Manage transformation of current piece into dead blocks, row deletion and score */
	record_dead_blocks(current_piece, board);

	int num_deleted = remove_and_count_full_rows(board, current_piece->max_y,
						     current_piece->min_y, board_win);

	score += score_from_lines(num_deleted);

	free(current_piece);

	/* Move the tetromino object from the preview window to the board */
	current_piece = preview_piece;
	
	reposition_tetromino(current_piece, SPAWN_HEIGHT, SPAWN_WIDTH,
			     preview_win, board_win);

	preview_piece = new_tetromino(random_type(), PREVIEW_WIN_SIDE / 2 - 1,
				      PREVIEW_WIN_SIDE / 2, preview_win);

	/* GAMEOVER CONDITION: the piece already collides with a dead piece as soon as it spawns */
        if (check_collision(current_piece, board) != NO_COLLISION) {
	  wrefresh(preview_win);
	  wrefresh(board_win);
          gameover = true;
        }

        continue;    /* Skip keyboard input during timed event management */
      }
    }
    
    /* Check for user event */
    chtype ch;
    if ((ch = getch()) != ERR) {

      if (toupper(ch) == 'W' || ch == KEY_UP) {
	rotate_tetromino(current_piece, board, board_win);
      } else if (toupper(ch) == 'A' || ch == KEY_LEFT) {
	move_tetromino(current_piece, board, 0, -1, board_win);
      } else if (toupper(ch) == 'S' || ch == KEY_DOWN) {
	move_tetromino(current_piece, board, +1, 0, board_win);
      } else if (toupper(ch) == 'D' || ch == KEY_RIGHT) {
	move_tetromino(current_piece, board, 0, +1, board_win);
      } else {
	gameover = ch == Ctrl('C'); /* Force-quit */
      }
    }
  }


  /* Gameover operations */

  if (top_score(rankings, score)) {
    char player_name[NAME_BUF_LEN];
    insert_ranking_name(player_name);
    record_ranking(rankings, player_name, score);
  }

  enum GameState next_state = manage_gameover();

  /* Cleanup */
  free(current_piece);
  free(preview_piece);
  free_gameboard(board);

  delwin(board_win);
  delwin(preview_win);
  delwin(side_win);
  delwin(field_win);
  
  return next_state;
}



void query_save_scores(void)
{
  /* NOT IMPLEMENTED */
}


int main(void)
{
  initialize();

  struct Ranking rankings[MAX_RANKINGS + 1] = INIT_RANKINGS;
  
  enum GameState next_state = STATE_TITLE;  
  
  while (true) {

    if (next_state == STATE_TITLE) {
      
      next_state = title_screen();
      
    } else if (next_state == STATE_GAME) {
      
      next_state = game_screen(rankings);
      
    } else if (next_state == STATE_SCORES) {
      
      next_state = score_screen(rankings);
      
    } else {			/* Quitting */
      
      query_save_scores();
      break;
    }
  }
}
