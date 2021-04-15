#ifndef H_TETRODROPPER_H
#define H_TETRODROPPER_H

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ncurses.h>


#define Ctrl(ch)	((ch) - 'A' + 1)

#define Min(a, b)	((a) < (b) ? (a) : (b))
#define Max(a, b)	((a) > (b) ? (a) : (b))

#define BOARD_HEIGHT	16
#define BOARD_WIDTH	10


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
  DEAD_TYPE
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


struct GameBoard {
  int		height;
  int		width;
  int		left_wall_x;
  int		right_wall_x;
  int		spawn_point_y;
  int		spawn_point_x;
  int		floor_y;
  int **	is_filled;
};


struct Tetromino {

  struct Point		square[4];
  int			min_y;
  int			center_y;
  int			max_y;
  int			min_x;
  int			center_x;
  int			max_x;
  enum TetrominoType	type;
  int			num_states;
  int			rotation_state;

};


/* Tetromino initializers */

#define I_TETROMINO_TEMPLATE						\
  (struct Tetromino){ .square = { {-1, 0}, {0, 0}, {1, 0}, {2, 0} },	\
      .min_y = -1, .center_y = 0, .max_y = 2,				\
      .min_x = 0, .center_x = 0, .max_x = 0,				\
      .type = I_TYPE, .num_states = 2, .rotation_state = 0 };
#define L_TETROMINO_TEMPLATE						\
  (struct Tetromino){ .square = { {-1, 0}, {0, 0}, {1, 0}, {1, -1} },	\
      .min_y = -1, .center_y = 0, .max_y = 1,				\
      .min_x = -1, .center_x = 0, .max_x = 0,				\
      .type = L_TYPE, .num_states = 4, .rotation_state = 0 };
#define J_TETROMINO_TEMPLATE						\
  (struct Tetromino){ .square = { {-1, 0}, {0, 0}, {1, 0}, {1, 1} },	\
      .min_y = -1, .center_y = 0, .max_y = 1,				\
      .min_x = 0, .center_x = 0, .max_x = 1,				\
      .type = J_TYPE, .num_states = 4, .rotation_state = 0 };
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
 * Game preparation
 */

void initialize(void);

void cleanup(void);


/* 
 * Game objects
 */

struct Tetromino *new_tetromino(enum TetrominoType type, int spawn_y, int spawn_x, WINDOW *win);

struct GameBoard *new_gameboard(int height, int width);

void free_gameboard(struct GameBoard *board);


/*
 * Functions
 */

enum CollisionType point_collision(struct Point p, struct GameBoard *board);

void rotate_tetromino(struct Tetromino *t, struct GameBoard *board, WINDOW *win);

void reposition_tetromino(struct Tetromino *t, int new_y, int new_x, WINDOW *from, WINDOW *to);

enum CollisionType move_tetromino(struct Tetromino *t, struct GameBoard *board, int dy, int dx,
				  WINDOW *win);


/*
 * Graphics
 */

void draw_tetromino(WINDOW *win, struct Tetromino * const t, int offset_y, int offset_x);

void delete_tetromino(WINDOW *win, struct Tetromino * const t, int offset_y, int offset_x);

void draw_board(WINDOW *win, int top_left_y, int top_left_x, int height, int width);

#endif	/* H_TETRODROPPER_H */
