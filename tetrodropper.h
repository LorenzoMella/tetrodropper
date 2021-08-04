#ifndef H_TETRODROPPER_H
#define H_TETRODROPPER_H

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ncurses.h>


#define Min(a, b)	((a) < (b) ? (a) : (b))
#define Max(a, b)	((a) > (b) ? (a) : (b))

#define Ctrl(ch)	((ch) - 'A' + 1)

#define BOARD_HEIGHT		16
#define BOARD_WIDTH		10
#define SPAWN_HEIGHT		1 /* Vertical displacement of the center of a new spawned piece */
#define SPAWN_WIDTH		(BOARD_WIDTH / 2) /* Horizontal alignment of new spawned piece */
#define PREVIEW_WIN_SIDE	7 /* Side length of the preview window */
#define MAX_TYPES		7 /* Number of distinct tetromino types */
#define MAX_BLOCKS		4 /* Number of blocks in a tetromino (as the name implies) */
#define TITLE_HEIGHT		4
#define TITLE_WIDTH		73

#ifdef NDEBUG

#  define INITIAL_SPEED		1.0
#  define SPEED_INCREMENT	(1.0 / 3.0)
#  define SCORE_MODULUS		1500 /* Points required for a speed increase */

#else

#  define INITIAL_SPEED		2.0
#  define SPEED_INCREMENT	2.0
#  define SCORE_MODULUS		300 /* Points required for a speed increase */

#endif


char title_string[TITLE_HEIGHT][1 + TITLE_WIDTH] = { /* If changed, match the lengths with the ASCII art! */
  " _____ _____ _____ _____ _____ ____  _____ _____ _____ _____ _____ _____ ",
  "|_   _|   __|_   _| __  |     |    \\| __  |     |  _  |  _  |   __| __  |",
  "  | | |   __| | | |    -|  |  |  |  |    -|  |  |   __|   __|   __|    -|",
  "  |_| |_____| |_| |__|__|_____|____/|__|__|_____|__|  |__|  |_____|__|__|"
};


/* Logger for managed crashes */
#define Die(__die_condition)						\
  do {									\
    if ((__die_condition)) {						\
      endwin();								\
      fprintf(stderr, "%s: %s: %d: %s\n", __FILE__, __func__, __LINE__,	\
              strerror(errno));						\
      exit(EXIT_FAILURE);						\
    }									\
  } while (0)  


enum CollisionType {
  NO_COLLISION,
  WALL_COLLISION,
  FLOOR_COLLISION,
  DEAD_BLOCK_COLLISION
};


enum TetrominoType {
  I_TYPE = 1,
  J_TYPE,
  L_TYPE,
  S_TYPE,
  Z_TYPE,
  O_TYPE,
  T_TYPE,
  DEAD_TYPE			/* Unused */
};


enum GameState {
  STATE_TITLE,
  STATE_GAME,
  STATE_SCORES,
  STATE_QUIT
};


struct Point {
    int y;
    int x;
  };


struct Ranking {
  char name[4];
  long score;
};

  
struct GameBoard {
  int		height;
  int		width;
  int		left_wall_x;
  int		right_wall_x;
  int		spawn_point_y;
  int		spawn_point_x;
  int		floor_y;
  bool **	is_filled;
};


struct Tetromino {
  struct Point		square[4];
  int			min_y;
  int			center_y;
  int			max_y;
  int			min_x;
  int			center_x;
  int			max_x;
  int			num_states;
  int			rotation_state;
  enum TetrominoType	type;
};


/* Rankings initializer */
#define INIT_RANKINGS				\
  {						\
    {.name = "AAA", .score = 0},		\
      {.name = "AAA", .score = 0},		\
      {.name = "AAA", .score = 0},		\
      {.name = "AAA", .score = 0},		\
      {.name = "AAA", .score = 0},		\
      {.name = "AAA", .score = 0},		\
      {.name = "AAA", .score = 0},		\
      {.name = "AAA", .score = 0},		\
      {.name = "AAA", .score = 0},		\
      {.name = "AAA", .score = 0},		\
      {.name = "???", .score = 0}		\
  }


/* Tetromino initializers */

#define I_TETROMINO_TEMPLATE						\
  (struct Tetromino){ .square = { {-1, 0}, {0, 0}, {1, 0}, {2, 0} },	\
      .min_y = -1, .center_y = 0, .max_y = 2,				\
      .min_x = 0, .center_x = 0, .max_x = 0,				\
      .type = I_TYPE, .num_states = 2, .rotation_state = 0 };
#define J_TETROMINO_TEMPLATE						\
  (struct Tetromino){ .square = { {-1, 0}, {0, 0}, {1, 0}, {1, -1} },	\
      .min_y = -1, .center_y = 0, .max_y = 1,				\
      .min_x = -1, .center_x = 0, .max_x = 0,				\
      .type = J_TYPE, .num_states = 4, .rotation_state = 0 };
#define L_TETROMINO_TEMPLATE						\
  (struct Tetromino){ .square = { {-1, 0}, {0, 0}, {1, 0}, {1, 1} },	\
      .min_y = -1, .center_y = 0, .max_y = 1,				\
      .min_x = 0, .center_x = 0, .max_x = 1,				\
      .type = L_TYPE, .num_states = 4, .rotation_state = 0 };
#define Z_TETROMINO_TEMPLATE						\
  (struct Tetromino){ .square = { {0, -1}, {0, 0}, {1, 0}, {1, 1} },	\
      .min_y = 0, .center_y = 0, .max_y = 1,				\
      .min_x = -1, .center_x = 0, .max_x = 1,				\
      .type = Z_TYPE, .num_states = 2, .rotation_state = 0 };
#define S_TETROMINO_TEMPLATE						\
  (struct Tetromino){ .square = { {1, -1}, {1, 0}, {0, 0}, {0, 1} },	\
      .min_y = 0, .center_y = 0, .max_y = 1,				\
      .min_x = -1, .center_x = 0, .max_x = 1,				\
      .type = S_TYPE, .num_states = 2, .rotation_state = 0 };
#define O_TETROMINO_TEMPLATE						\
  (struct Tetromino){ .square = { {-1, -1}, {-1, 0}, {0, -1}, {0, 0} },	\
      .min_y = -1, .center_y = 0, .max_y = 0,				\
      .min_x = -1, .center_x = 0, .max_x = 0,				\
      .type = O_TYPE, .num_states = 1, .rotation_state = 0 };
#define T_TETROMINO_TEMPLATE						\
  (struct Tetromino){ .square = { {0, -1}, {0, 0}, {0, 1}, {-1, 0} },	\
      .min_y = -1, .center_y = 0, .max_y = 0,				\
      .min_x = -1, .center_x = 0, .max_x = 1,				\
      .type = T_TYPE, .num_states = 4, .rotation_state = 0 };



/* 
 * Terminal preparation
 */


void initialize(void);

void cleanup(void);



/* 
 * Game objects
 */


enum TetrominoType random_type(void);
  
struct Tetromino *new_tetromino(enum TetrominoType type, int spawn_y, int spawn_x, WINDOW *win);

struct GameBoard *new_gameboard(int height, int width);

void free_gameboard(struct GameBoard *board);



/*
 * Game Mechanics
 */


void recompute_bounding_box(struct Tetromino *t);

enum CollisionType point_collision(struct Point p, struct GameBoard *board);

enum CollisionType check_collision(struct Tetromino *t, struct GameBoard *board);

struct Point rotate_point_90(struct Point p, int origin_y, int origin_x, bool counter_clockwise);

enum CollisionType rotate_tetromino(struct Tetromino *t, struct GameBoard *board, WINDOW *win);

enum CollisionType move_tetromino(struct Tetromino *t, struct GameBoard *board, int dy, int dx,
				  WINDOW *win);

void reposition_tetromino(struct Tetromino *t, int new_y, int new_x, WINDOW *from, WINDOW *to);

void record_dead_blocks(struct Tetromino *t, struct GameBoard *board);

bool row_is_full(struct GameBoard *board, int row);

int remove_and_count_full_rows(struct GameBoard *board, int bottom_row, int top_row, WINDOW *win);

long score_from_lines(int num_lines);

double speed_from_score(long score);

double get_real_time(void);



/*
 * Graphics
 */


void draw_tetromino(WINDOW *win, struct Tetromino * const t, int offset_y, int offset_x);

void delete_tetromino(WINDOW *win, struct Tetromino * const t, int offset_y, int offset_x);

void draw_board(WINDOW *win, int top_left_y, int top_left_x, int height, int width);

void animate_drop(WINDOW *win, int row);

void draw_updated_stats(WINDOW *win, long score, double speed);

WINDOW *draw_message_popup(char *msg);


/*
 * Game Phases
 */
  

enum GameState title_screen(void);

enum GameState score_screen(struct Ranking rankings[11]);

enum GameState manage_gameover(void);

enum GameState game_screen(struct Ranking rankings[11]);

void query_save_scores(void);



#endif	/* H_TETRODROPPER_H */
