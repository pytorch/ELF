/**
 * Copyright (c) 2018-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "board.h"
#include <iostream>
#include <vector>

#define myassert(p, text) \
  do {                    \
    if (!(p)) {           \
      printf((text));     \
    }                     \
  } while (0)
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#pragma message "BOARD_SIZE = " __STR(__MACRO_BOARD_SIZE)

uint64_t transform_hash(uint64_t h, Stone s) {
  switch (s) {
    case S_EMPTY:
    case S_OFF_BOARD:
      return 0;
    case S_BLACK:
      return h;
    case S_WHITE:
      return (h >> 32) | ((h & ((1ULL << 32) - 1)) << 32);
    default:
      return h;
  }
}

inline void set_color(Board* board, Coord c, Stone s) {
  Stone old_s = board->_infos[c].color;
  board->_infos[c].color = s;

  unsigned char offset = ((c & 3) << 1);
  unsigned char mask = ~(3 << offset);
  board->_bits[c >> 2] &= mask;
  board->_bits[c >> 2] |= (s << offset);

  uint64_t h = _board_hash[c];

  board->_hash ^= transform_hash(h, old_s);
  board->_hash ^= transform_hash(h, s);
}

bool isBitsEqual(const Board::Bits bits1, const Board::Bits bits2) {
  for (size_t i = 0; i < sizeof(Board::Bits) / sizeof(unsigned char); ++i) {
    if (bits1[i] != bits2[i])
      return false;
  }
  return true;
}

void copyBits(Board::Bits bits_dst, const Board::Bits bits_src) {
  ::memcpy((void*)bits_dst, (const void*)bits_src, sizeof(Board::Bits));
}

// Functions..
void setAsBorder(Board* board, int /*side*/, int i1, int w, int j1, int h) {
  for (int i = i1; i < i1 + w; i++) {
    for (int j = j1; j < j1 + h; ++j) {
      if (i < 0 || i >= BOARD_EXPAND_SIZE || j < 0 || j >= BOARD_EXPAND_SIZE) {
        printf("Fill: (%d, %d) is out of bound!", i, j);
      }
      Coord c = EXTENDOFFSETXY(i, j);
      set_color(board, c, S_OFF_BOARD);
      board->_infos[c].id = MAX_GROUP;
    }
  }
}

void clearBoard(Board* board) {
  // The initial hash is zero.
  memset((void*)board, 0, sizeof(Board));
  // Setup the offboard mark.
  setAsBorder(board, BOARD_EXPAND_SIZE, 0, BOARD_MARGIN, 0, BOARD_EXPAND_SIZE);
  setAsBorder(
      board,
      BOARD_EXPAND_SIZE,
      BOARD_SIZE + BOARD_MARGIN,
      BOARD_MARGIN,
      0,
      BOARD_EXPAND_SIZE);
  setAsBorder(board, BOARD_EXPAND_SIZE, 0, BOARD_EXPAND_SIZE, 0, BOARD_MARGIN);
  setAsBorder(
      board,
      BOARD_EXPAND_SIZE,
      0,
      BOARD_EXPAND_SIZE,
      BOARD_SIZE + BOARD_MARGIN,
      BOARD_MARGIN);
  board->_next_player = S_BLACK;
  board->_last_move = M_INVALID;
  board->_last_move2 = M_INVALID;
  board->_last_move3 = M_INVALID;
  board->_last_move4 = M_INVALID;
  board->_num_groups = 1;
  // The initial ply number is 1.
  board->_ply = 1;
}

bool PlaceHandicap(Board* board, int x, int y, Stone player) {
  // If the game has already started, return false.
  if (board->_ply > 1)
    return false;
  GroupId4 ids;
  if (TryPlay(board, x, y, player, &ids)) {
    Play(board, &ids);
    // Keep the board situations.
    board->_ply = 1;
    board->_last_move = M_INVALID;
    board->_last_move2 = M_INVALID;
    board->_last_move3 = M_INVALID;
    board->_last_move4 = M_INVALID;
    return true;
  }
  // Not a valid move.
  return false;
}

void copyBoard(Board* dst, const Board* src) {
  myassert(dst, "dst cannot be nullptr");
  myassert(src, "src cannot be nullptr");
  memcpy(dst, src, sizeof(Board));
}

bool compareBoard(const Board* b1, const Board* b2) {
  // Compare them per byte.
  unsigned char* p1 = (unsigned char*)b1;
  unsigned char* p2 = (unsigned char*)b2;

  for (size_t i = 0; i < sizeof(Board); ++i) {
    if (p1[i] != p2[i])
      return false;
  }
  return true;
}

/*
static void printGroupId4(const GroupId4 *ids) {
  char buf[30];
  printf("Move = %s, Liberty = %d\n", get_move_str(ids->c, ids->player, buf),
ids->liberty);
  for (int i = 0; i < 4; ++i) {
    printf("[%d]: %d, %d, %d\n", i, ids->ids[i], ids->colors[i],
ids->group_liberties[i]);
  }
}
*/

// board analysis, whether putting or removing this stone will yield a change in
// the liberty in the surrounding group,
// Also we could get the liberty of that stone as well.
static inline void
StoneLibertyAnalysis(const Board* board, Stone player, Coord c, GroupId4* ids) {
  memset(ids, 0, sizeof(GroupId4));
  ids->c = c;
  ids->player = player;
  // print("Analysis at (%d, %d)", X(c), Y(c));
  FOR4(c, i4, c4) {
    unsigned short group_id = board->_infos[c4].id;
    if (G_EMPTY(group_id)) {
      ids->liberty++;
      continue;
    }
    if (!G_ONBOARD(group_id))
      continue;

    // simple way to check duplicate group ids
    bool visited_before = false;
    // Unrolling.
    visited_before += (ids->ids[0] == group_id);
    visited_before += (ids->ids[1] == group_id);
    visited_before += (ids->ids[2] == group_id);
    // no need to compare agasint ids[3]
    /*
    for (int j = 0; j < i4; ++j) {
      if (ids->ids[j] == group_id) {
        visited_before = true;
        break;
      }
    }
    */
    if (visited_before)
      continue;
    // No we could say this location will change the liberty of the group.
    ids->ids[i4] = group_id;
    ids->colors[i4] = board->_groups[group_id].color;
    ids->group_liberties[i4] = board->_groups[group_id].liberties;
  }
  ENDFOR4
}

static inline bool isSuicideMove(const GroupId4* ids) {
  // Prevent any suicide moves.
  if (ids->liberty > 0)
    return false;

  int cnt_our_group_liberty_more_than_1 = 0;
  int cnt_enemy_group_liberty_1 = 0;
  for (int i = 0; i < 4; ++i) {
    if (ids->ids[i] == 0)
      continue;
    // printf("isSuicideMove: player = %d, Group id = %d, color = %d, liberty =
    // %d\n", player, ids->ids[i], ids->colors[i], ids->group_liberties[i]);
    if (ids->colors[i] == ids->player) {
      if (ids->group_liberties[i] > 1)
        cnt_our_group_liberty_more_than_1++;
    } else {
      if (ids->group_liberties[i] == 1)
        cnt_enemy_group_liberty_1++;
    }
  }

  // If the following conditions holds, then it is a suicide move:
  // 1. There is no friendly group (or all friendly groups has only one
  // liberty),
  // 2. Our own liberty is zero.
  // 3. All enemy liberties are great than 1, which means we cannot kill any
  // enemy groups.
  if (cnt_our_group_liberty_more_than_1 == 0 && cnt_enemy_group_liberty_1 == 0)
    return true;

  return false;
}

static inline bool isSimpleKoViolation(const Board* b, Coord c, Stone player) {
  if (b->_simple_ko == c && b->_ko_age == 0 && b->_simple_ko_color == player) {
    // printf("Ko violations!!  (%d, %d), player = %d\n", X(c), Y(c), player);
    return true;
  } else
    return false;
}

bool isSelfAtariXY(
    const Board* board,
    const GroupId4* ids,
    int x,
    int y,
    Stone player,
    int* num_stones) {
  return isSelfAtari(board, ids, OFFSETXY(x, y), player, num_stones);
}

// If num_stones is not nullptr, return the number of stones for the
// to-be-formed atari group.
bool isSelfAtari(
    const Board* board,
    const GroupId4* ids,
    Coord c,
    Stone player,
    int* num_stones) {
  if (board == nullptr)
    error("SelfAtari: board cannot be nullptr!\n");
  GroupId4 ids2;
  if (ids == nullptr) {
    // Then we should run TryPlay2 by ourself.
    if (!TryPlay(board, X(c), Y(c), player, &ids2))
      return false;
    ids = &ids2;
  }
  // This stone has lots of liberties, not self-atari.
  if (ids->liberty >= 2)
    return false;

  // Self-Atari is too complicated, it is just better to mimic the move and
  // check the liberties.
  // If one of the self group has > 2 liberty, it is definitely not self-atari.
  for (int i = 0; i < 4; ++i) {
    if (ids->ids[i] != 0 && ids->colors[i] == player) {
      if (ids->group_liberties[i] > 2)
        return false;
    }
  }

  // Then we duplicate a board and check.
  Board b2;
  copyBoard(&b2, board);
  Play(&b2, ids);
  // showBoard(&b2, SHOW_LAST_MOVE);

  short id = b2._infos[c].id;
  if (b2._groups[id].liberties == 1) {
    if (num_stones != nullptr)
      *num_stones = b2._groups[id].stones;
    return true;
  } else {
    return false;
  }
}

#define MAX_LADDER_SEARCH 1024
int checkLadderUseSearch(Board* board, Stone victim, int* num_call, int depth) {
  (*num_call)++;
  Coord c = board->_last_move;
  Coord c2 = board->_last_move2;
  unsigned short id = board->_infos[c].id;
  unsigned short lib = board->_groups[id].liberties;
  // char buf[30];
  GroupId4 ids;

  if (victim == OPPONENT(board->_next_player)) {
    // Capturer to play. He can choose two ways to capture.

    // Captured.
    if (lib == 1)
      return depth;
    // Not able to capture.
    if (lib >= 3)
      return 0;
    // Check if c's vicinity's two empty locations.
    Coord escape[2];
    int num_escape = 0;
    FOR4(c, _, cc) {
      if (board->_infos[cc].color == S_EMPTY) {
        escape[num_escape++] = cc;
      }
    }
    ENDFOR4
    // Not a ladder.
    if (num_escape <= 1)
      return 0;

    // Play each possibility and recurse.
    // We can avoid copying if we are sure one branch cannot be right and only
    // trace down the other.
    int freedom[2];
    Coord must_block = M_PASS;
    for (int i = 0; i < 2; ++i) {
      freedom[i] = 0;
      FOR4(escape[i], _, cc) {
        if (board->_infos[cc].color == S_EMPTY) {
          freedom[i]++;
        }
      }
      ENDFOR4
      if (freedom[i] == 3) {
        // Then we have to block this.
        must_block = escape[i];
        break;
      }
    }

    // Check if we have too many branches. If so, stopping the branching.
    if (must_block == M_PASS && *num_call >= MAX_LADDER_SEARCH) {
      must_block = escape[0];
    }

    if (must_block != M_PASS) {
      // It suffices to only play must_block.
      if (TryPlay2(board, must_block, &ids)) {
        Play(board, &ids);
        int final_depth =
            checkLadderUseSearch(board, victim, num_call, depth + 1);
        if (final_depth > 0)
          return final_depth;
      }
    } else {
      // printf("isLadderUseSearch: Branching:\n");
      // printf("Choice 1: %s\n", get_move_str(escape[0], board->_next_player,
      // buf));
      // printf("Choice 2: %s\n", get_move_str(escape[1], board->_next_player,
      // buf));
      // showBoard(board, SHOW_ALL);

      // We need to play both. This should seldomly happen.
      Board b_next;
      copyBoard(&b_next, board);
      if (TryPlay2(&b_next, escape[0], &ids)) {
        Play(&b_next, &ids);
        int final_depth =
            checkLadderUseSearch(&b_next, victim, num_call, depth + 1);
        if (final_depth > 0)
          return final_depth;
      }

      if (TryPlay2(board, escape[1], &ids)) {
        Play(board, &ids);
        int final_depth =
            checkLadderUseSearch(board, victim, num_call, depth + 1);
        if (final_depth > 0)
          return final_depth;
      }
    }
  } else {
    // Victim to play. In general he only has one choice because he is always in
    // atari.
    // If the capturer place a stone in atari, then the capture fails.
    if (lib == 1)
      return 0;
    // Otherwise the victim need to continue fleeing.
    Coord flee_loc = M_PASS;
    FOR4(c2, _, cc) {
      if (board->_infos[cc].color == S_EMPTY) {
        flee_loc = cc;
        break;
      }
    }
    ENDFOR4
    // Make sure flee point is not empty
    if (flee_loc == M_PASS) {
      showBoard(board, SHOW_ALL);
      error("Error!! isLadderUseSearch is wrong!\n");
      return 0;
    }
    if (TryPlay2(board, flee_loc, &ids)) {
      Play(board, &ids);
      unsigned char id = board->_infos[flee_loc].id;
      if (board->_groups[id].liberties >= 3)
        return 0;
      if (board->_groups[id].liberties == 2) {
        // Check if the neighboring enemy stone has only one liberty, if so,
        // then it is not a ladder.
        FOR4(flee_loc, _, cc) {
          if (board->_infos[cc].color != OPPONENT(victim))
            continue;
          unsigned char id2 = board->_infos[cc].id;
          // If the enemy group is in atari but our group has 2 liberties, then
          // it is not a ladder.
          if (board->_groups[id2].liberties == 1)
            return 0;
        }
        ENDFOR4
      }
      int final_depth =
          checkLadderUseSearch(board, victim, num_call, depth + 1);
      if (final_depth > 0)
        return final_depth;
    }
  }
  return 0;
}

// Whether the move to be checked is a simple ko move.
bool isMoveGivingSimpleKo(
    const Board* board,
    const GroupId4* ids,
    Stone player) {
  // Check if
  // 1. the move is surrounded by enemy groups
  // 2. One enemy group has only one liberty and its size is 1.
  if (ids->liberty > 0)
    return false;

  int cnt_enemy_group_liberty1_size1 = 0;
  for (int i = 0; i < 4; ++i) {
    if (ids->ids[i] == 0)
      continue;
    if (ids->colors[i] == player)
      return false;
    const Group* g = &board->_groups[ids->ids[i]];
    if (ids->group_liberties[i] == 1 && g->stones == 1)
      cnt_enemy_group_liberty1_size1++;
  }

  return cnt_enemy_group_liberty1_size1 == 1 ? true : false;
}

Coord getSimpleKoLocation(const Board* board, Stone* player) {
  if (board->_ko_age == 0 && board->_simple_ko != M_PASS) {
    if (player != nullptr)
      *player = board->_simple_ko_color;
    return board->_simple_ko;
  } else {
    return M_PASS;
  }
}

// Simple ladder check.
// Return 0 if there is no ladder, otherwise return the depth of the ladder.
int checkLadder(const Board* board, const GroupId4* ids, Stone player) {
  // Check if the victim's move will lead to a ladder.
  if (ids->liberty != 2)
    return 0;
  // Count the number of enemy groups, must be exactly one.
  int num_of_enemy = 0;
  int num_of_self = 0;
  bool one_enemy_three = false;
  bool one_in_atari = false;
  for (int i = 0; i < 4; ++i) {
    unsigned char id = ids->ids[i];
    if (id == 0)
      continue;
    if (ids->colors[i] == OPPONENT(player)) {
      if (num_of_enemy >= 1) {
        one_enemy_three = false;
      } else {
        if (ids->group_liberties[i] >= 3)
          one_enemy_three = true;
      }
      num_of_enemy++;
    } else {
      // One and only one group has one liberty.
      if (num_of_self >= 1) {
        one_in_atari = false;
      } else {
        if (ids->group_liberties[i] == 1)
          one_in_atari = true;
      }
      num_of_self++;
    }
  }
  if (one_enemy_three && one_in_atari) {
    // Then we do expensive check.
    // printf("isLadder: Expensive check start...\n");
    Board b_next;
    copyBoard(&b_next, board);

    // Play victim's move.
    Play(&b_next, ids);
    // Check whether it will lead to ladder.
    int num_call = 0;
    int depth = 1;
    return checkLadderUseSearch(&b_next, player, &num_call, depth);
  }
  return 0;
}

void RemoveStoneAndAddLiberty(Board* board, Coord c) {
  // First perform an analysis.
  GroupId4 ids;
  StoneLibertyAnalysis(board, board->_next_player, c, &ids);

  // Check nearby groups and add their liberties. Note that we need to skip our
  // own group (since it will be removed eventually).
  for (int i = 0; i < 4; ++i) {
    unsigned short id = ids.ids[i];
    if (id == 0 || id == board->_infos[c].id)
      continue;
    board->_groups[id].liberties++;
  }

  // printf("RemoveStoneAndAddLiberty: Remove stone at (%d, %d), belonging to
  // Group %d\n", X(c), Y(c), board->_infos[c].id);
  set_color(board, c, S_EMPTY);
  board->_infos[c].id = 0;
  board->_infos[c].next = 0;
}

// Group related opreations.
bool EmptyGroup(Board* board, unsigned short group_id) {
  if (group_id == 0)
    return false;
  Coord c = board->_groups[group_id].start;
  while (c != 0) {
    // printf("Remove stone (%d, %d)\n", X(c), Y(c));
    Coord next = board->_infos[c].next;
    RemoveStoneAndAddLiberty(board, c);
    c = next;
  }
  // Note this group might be visited again in RemoveAllEmptyGroups, if:
  // There are two empty groups, one with id and the other is the last group.
  // Then when we copy the last group to the former id, we might visit the last
  // group
  // again with invalid start pointer. This is bad.
  // However, if _empty_group_ids will be sorted, then it doesn't matter.
  // board->_groups[group_id].start = 0;
  board->_removed_group_ids[board->_num_group_removed++] = group_id;
  // empty_group_ids[(*next_empty_group) ++] = group_id;
  // board->_empty_group_ids[board->_next_empty_group++] = group_id;
  if (board->_num_group_removed > 4) {
    error("Error! _next_empty_group > 4!! \n");
  }
  return true;
}

void SimpleSort(unsigned char* ids, int n) {
  // Sort a vector in descending order.
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      if (ids[i] < ids[j]) {
        unsigned short tmp = ids[i];
        ids[i] = ids[j];
        ids[j] = tmp;
      }
    }
  }
}

void RemoveAllEmptyGroups(Board* board) {
  // A simple sorting on the empty group id.
  SimpleSort(board->_removed_group_ids, board->_num_group_removed);

  for (int i = 0; i < board->_num_group_removed; ++i) {
    unsigned short id = board->_removed_group_ids[i];
    // printf("Remove empty group %d\n", id);
    unsigned short last_id = board->_num_groups - 1;
    if (id != last_id) {
      // Swap with the last entry.
      // Copy the structure.
      memcpy(&board->_groups[id], &board->_groups[last_id], sizeof(Group));
      TRAVERSE(board, id, c) {
        board->_infos[c].id = id;
      }
      ENDTRAVERSE
      // No need to map ids once the ids are sorted in an descending order.
      // Check the following empty group ids, if they are at the end, map their
      // id to the new position.
      /*
      for (int j = i + 1; j < board->_next_empty_group; ++j) {
         if (board->_empty_group_ids[j] == last_id) {
           board->_empty_group_ids[j] = id;
           break;
         }
      }*/
    }
    board->_num_groups--;
  }
  // Clear _num_group_removed when play starts.
  // board->_next_empty_group = 0;
}

int getGroupReplaceSeq(
    const Board* board,
    unsigned char removed[4],
    unsigned char replaced[4]) {
  // Get the group remove/replaced seuqnece
  int last_before_removal = board->_num_group_removed + board->_num_groups - 1;
  for (int i = 0; i < board->_num_group_removed; ++i) {
    removed[i] = board->_removed_group_ids[i];
    if (last_before_removal == removed[i])
      replaced[i] = 0;
    else
      replaced[i] = last_before_removal;
    last_before_removal--;
  }
  return board->_num_group_removed;
}

// Convert old id to new id.
unsigned char BoardIdOld2New(const Board* board, unsigned char id) {
  // Get the group remove/replaced seuqnece
  int last_before_removal = board->_num_group_removed + board->_num_groups - 1;
  for (int i = 0; i < board->_num_group_removed; ++i) {
    // 0 mean it is removed.
    if (board->_removed_group_ids[i] == id)
      return 0;
    if (last_before_removal == id)
      id = board->_removed_group_ids[i];
    last_before_removal--;
  }
  return id;
}

/*
bool EmptyGroupAt(Board *board, Coord c) {
   if (!HAS_STONE(board->_info[c].color)) return false;
   unsigned short group_id = board->_infos[c].id;
   EmptyGroup(board, group_id);
   return false;
}
*/

unsigned short createNewGroup(Board* board, Coord c, int liberty) {
  unsigned short id = board->_num_groups++;
  board->_groups[id].color = board->_infos[c].color;
  board->_groups[id].start = c;
  board->_groups[id].liberties = liberty;
  board->_groups[id].stones = 1;

  board->_infos[c].id = id;
  board->_infos[c].next = 0;
  return id;
}

// Merge a single stone into an existing group. In this case, no group
// deletion/move
// is needed.
// Here the liberty is that of the single stone (raw liberty).
bool MergeToGroup(Board* board, Coord c, unsigned short id) {
  // Place the stone.
  set_color(board, c, board->_groups[id].color);
  board->_infos[c].last_placed = board->_ply;

  board->_infos[c].id = id;
  // Put the new stone to the beginning of the group.
  board->_infos[c].next = board->_groups[id].start;
  board->_groups[id].start = c;
  board->_groups[id].stones++;
// We need to be careful about the liberty, since some liberties of the new
// stone may also be liberty of the group to be merged.
#define SAME_ID(c) (board->_infos[(c)].id == id)

  bool lt = !SAME_ID(GO_LT(c));
  bool lb = !SAME_ID(GO_LB(c));
  bool rt = !SAME_ID(GO_RT(c));
  bool rb = !SAME_ID(GO_RB(c));

  if (EMPTY(board->_infos[GO_L(c)].color) && lt && lb && !SAME_ID(GO_LL(c)))
    board->_groups[id].liberties++;
  if (EMPTY(board->_infos[GO_R(c)].color) && rt && rb && !SAME_ID(GO_RR(c)))
    board->_groups[id].liberties++;
  if (EMPTY(board->_infos[GO_T(c)].color) && lt && rt && !SAME_ID(GO_TT(c)))
    board->_groups[id].liberties++;
  if (EMPTY(board->_infos[GO_B(c)].color) && lb && rb && !SAME_ID(GO_BB(c)))
    board->_groups[id].liberties++;

#undef SAME_ID

  return true;
}

// Merge two groups into one.
// The resulting liberties might not be right and need to be recomputed.
unsigned short
MergeGroups(Board* board, unsigned short id1, unsigned short id2) {
  // printf("merge beteween %d and %d", id1, id2);
  // Same id, no merge.
  if (id1 == id2)
    return id1;

  // To save computation power, we want to traverse through the group with small
  // number of stones.
  if (board->_groups[id2].stones > board->_groups[id1].stones)
    return MergeGroups(board, id2, id1);

  // Merge
  // Find the last stone in id2.
  Coord last_c_in_id2 = 0;
  TRAVERSE(board, id2, c) {
    board->_infos[c].id = id1;
    last_c_in_id2 = c;
  }
  ENDTRAVERSE

  // Make connections. Put id2 group in front of id1.
  board->_infos[last_c_in_id2].next = board->_groups[id1].start;
  board->_groups[id1].start = board->_groups[id2].start;
  //
  // Other quantities.
  board->_groups[id1].stones += board->_groups[id2].stones;
  // Note that the summed liberties is not right (since multiple groups might
  // share liberties, therefore we need to recompute it).
  board->_groups[id1].liberties = -1;

  // Make id2 an empty group.
  board->_groups[id2].start = 0;
  board->_removed_group_ids[board->_num_group_removed++] = id2;
  // board->_empty_group_ids[board->_next_empty_group++] = id2;

  // Equivalently, we could do:
  // board->_groups[id2].start = 0;
  // EmptyGroup(board, id2);
  return id1;
}

bool RecomputeGroupLiberties(Board* board, unsigned short id) {
  // Put all neighboring spaces into a set, and count the number.
  // Borrowing _info.next for counting. No extra space needed.
  if (id == 0)
    return false;
  short liberty = 0;
  TRAVERSE(board, id, c){FOR4(c, _, c4){Info* info = &board->_infos[c4];
  if (G_EMPTY(info->id) && info->next == 0) {
    // printf("RecomputeGroupLiberties: liberties of Group %d at (%d, %d)",
    // id, X(c4), Y(c4));
    info->next = 1;
    liberty++;
  }
}
ENDFOR4
}
ENDTRAVERSE
// Second traverse to set all .next to be zero.
TRAVERSE(board, id, c){FOR4(c, _, c4){Info* info = &board->_infos[c4];
if (G_EMPTY(info->id))
  info->next = 0;
}
ENDFOR4
}
ENDTRAVERSE

board->_groups[id].liberties = liberty;
return true;
}

bool TryPlay2(const Board* board, Coord m, GroupId4* ids) {
  return TryPlay(board, X(m), Y(m), board->_next_player, ids);
}

bool TryPlay(const Board* board, int x, int y, Stone player, GroupId4* ids) {
  // Place the stone on the coordinate, and update other structures.
  myassert(board, "TryPlay: Board is nil!");
  myassert(ids, "TryPlay: GroupIds4 is nil!");

  Coord c = OFFSETXY(x, y);
  if (c == M_PASS || c == M_RESIGN) {
    memset(ids, 0, sizeof(GroupId4));
    ids->c = c;
    ids->player = player;

    return true;
  }

  // Return false if the move is out of bound.
  if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE)
    return false;

  // printf("Move: (%d, %d), player = %d\n", X(c), Y(c), player);
  // Cannot place a move here.
  if (!EMPTY(board->_infos[c].color)) {
    // printf("Already has stone! (%d, %d) with color = %d (player color =
    // %d)\n", X(c), Y(c), board->_infos[c].color, player);
    return false;
  }

  // Prevent any simple ko violation..
  if (isSimpleKoViolation(board, c, player))
    return false;

  // Perform an analysis.
  StoneLibertyAnalysis(board, player, c, ids);
  // Prevent any suicide move.
  if (isSuicideMove(ids)) {
    // printf("Is suicide move! (%d, %d), player = %d\n", X(c), Y(c), player);
    return false;
  }

  return true;
}

void getAllStones(const Board* board, AllMoves* black, AllMoves* white) {
  black->num_moves = 0;
  white->num_moves = 0;

  black->board = board;
  white->board = board;

  // Get all stones from the board.
  for (int x = 0; x < BOARD_SIZE; ++x) {
    for (int y = 0; y < BOARD_SIZE; ++y) {
      Coord c = OFFSETXY(x, y);
      Stone s = board->_infos[c].color;
      if (s == S_BLACK) {
        black->moves[black->num_moves++] = c;
      } else if (s == S_WHITE) {
        white->moves[white->num_moves++] = c;
      }
    }
  }
}

void FindAllCandidateMoves(
    const Board* board,
    Stone player,
    int self_atari_thres,
    AllMoves* all_moves) {
  GroupId4 ids;
  Coord c;
  all_moves->board = board;
  all_moves->num_moves = 0;
  int self_atari_count = 0;
  for (int x = 0; x < BOARD_SIZE; ++x) {
    for (int y = 0; y < BOARD_SIZE; ++y) {
      c = OFFSETXY(x, y);
      if (!EMPTY(board->_infos[c].color))
        continue;
      StoneLibertyAnalysis(board, player, c, &ids);

      // It is illegal to play at ko locations.
      if (isSimpleKoViolation(board, c, player))
        continue;

      // It is illegal to play a suicide move.
      if (isSuicideMove(&ids))
        continue;

      // Never fill a true eye.
      if (isTrueEye(board, c, player))
        continue;

      // Be careful about self-atari moves.
      if (isSelfAtari(board, &ids, c, player, &self_atari_count)) {
        // For self-atari's with fewer counts, we could tolorate since they are
        // usually important in killing others' group.
        if (self_atari_count >= self_atari_thres)
          continue;
      }

      all_moves->moves[all_moves->num_moves++] = c;
    }
  }
}

void FindAllCandidateMovesInRegion(
    const Board* board,
    const Region* r,
    Stone player,
    int self_atari_thres,
    AllMoves* all_moves) {
  GroupId4 ids;
  Coord c;
  all_moves->board = board;
  all_moves->num_moves = 0;
  int self_atari_count = 0;

  int left, top, right, bottom;
  if (r == nullptr) {
    left = 0;
    top = 0;
    right = BOARD_SIZE;
    bottom = BOARD_SIZE;
  } else {
    left = r->left;
    top = r->top;
    right = r->right;
    bottom = r->bottom;
  }

  for (int x = left; x < right; ++x) {
    for (int y = top; y < bottom; ++y) {
      c = OFFSETXY(x, y);
      if (!EMPTY(board->_infos[c].color))
        continue;
      StoneLibertyAnalysis(board, player, c, &ids);

      // It is illegal to play at ko locations.
      if (isSimpleKoViolation(board, c, player))
        continue;

      // It is illegal to play a suicide move.
      if (isSuicideMove(&ids))
        continue;

      // Never fill a true eye.
      if (isTrueEye(board, c, player))
        continue;

      // Be careful about self-atari moves.
      if (isSelfAtari(board, &ids, c, player, &self_atari_count)) {
        // For self-atari's with fewer counts, we could tolorate since they are
        // usually important in killing others' group.
        if (self_atari_count >= self_atari_thres)
          continue;
      }

      all_moves->moves[all_moves->num_moves++] = c;
    }
  }
}

void FindAllValidMoves(const Board* board, Stone player, AllMoves* all_moves) {
  GroupId4 ids;
  Coord c;
  all_moves->board = board;
  all_moves->num_moves = 0;
  for (int x = 0; x < BOARD_SIZE; ++x) {
    for (int y = 0; y < BOARD_SIZE; ++y) {
      c = OFFSETXY(x, y);
      if (!EMPTY(board->_infos[c].color))
        continue;
      StoneLibertyAnalysis(board, player, c, &ids);
      if (isSimpleKoViolation(board, c, player))
        continue;
      if (isSuicideMove(&ids))
        continue;

      all_moves->moves[all_moves->num_moves++] = c;
    }
  }
}

void FindAllValidMovesInRegion(
    const Board* board,
    const Region* r,
    AllMoves* all_moves) {
  int left, top, right, bottom;
  if (r == nullptr) {
    left = 0;
    top = 0;
    right = BOARD_SIZE;
    bottom = BOARD_SIZE;
  } else {
    left = r->left;
    top = r->top;
    right = r->right;
    bottom = r->bottom;
  }

  GroupId4 ids;
  Coord c;
  all_moves->board = board;
  all_moves->num_moves = 0;
  for (int x = left; x < right; ++x) {
    for (int y = top; y < bottom; ++y) {
      c = OFFSETXY(x, y);
      if (!EMPTY(board->_infos[c].color))
        continue;
      StoneLibertyAnalysis(board, board->_next_player, c, &ids);
      if (isSimpleKoViolation(board, c, board->_next_player))
        continue;
      if (isSuicideMove(&ids))
        continue;

      all_moves->moves[all_moves->num_moves++] = c;
    }
  }
}

bool isIn(const Region* r, Coord c) {
  int x = X(c);
  int y = Y(c);
  return r->left <= x && r->top <= y && x < r->right && y < r->bottom ? true
                                                                      : false;
}

void Expand(Region* r, Coord c) {
  int x = X(c);
  int y = Y(c);

  r->left = min(r->left, x);
  r->top = min(r->top, y);
  r->right = max(r->right, x + 1);
  r->bottom = max(r->bottom, y + 1);
}

void getBoardBBox(const Board* board, Region* r) {
  myassert(r, "Input region cannot be nullptr!");
  // Get the bounding box that covers the stones.
  r->left = BOARD_SIZE;
  r->top = BOARD_SIZE;
  r->right = 0;
  r->bottom = 0;

  for (int i = 1; i < board->_num_groups; ++i) {
    TRAVERSE(board, i, c) {
      Expand(r, c);
    }
    ENDTRAVERSE
  }
}

Stone GuessLDAttacker(const Board* board, const Region* r) {
  // Do a scanning.
  int white_count = 0;
  int black_count = 0;

  if (r->left > 0) {
    for (int j = r->top; j < r->bottom; ++j) {
      for (int i = r->left; i < r->right; ++i) {
        Stone s = board->_infos[OFFSETXY(i, j)].color;
        if (s == S_BLACK) {
          black_count++;
          break;
        } else if (s == S_WHITE) {
          white_count++;
          break;
        }
      }
    }
  }

  if (r->top > 0) {
    for (int i = r->left; i < r->right; ++i) {
      for (int j = r->top; j < r->bottom; ++j) {
        Stone s = board->_infos[OFFSETXY(i, j)].color;
        if (s == S_BLACK) {
          black_count++;
          break;
        } else if (s == S_WHITE) {
          white_count++;
          break;
        }
      }
    }
  }

  if (r->right < BOARD_SIZE) {
    for (int j = r->top; j < r->bottom; ++j) {
      for (int i = r->right - 1; i >= r->left; --i) {
        Stone s = board->_infos[OFFSETXY(i, j)].color;
        if (s == S_BLACK) {
          black_count++;
          break;
        } else if (s == S_WHITE) {
          white_count++;
          break;
        }
      }
    }
  }

  if (r->bottom < BOARD_SIZE) {
    for (int i = r->left; i < r->right; ++i) {
      for (int j = r->bottom - 1; j >= r->top; --j) {
        Stone s = board->_infos[OFFSETXY(i, j)].color;
        if (s == S_BLACK) {
          black_count++;
          break;
        } else if (s == S_WHITE) {
          white_count++;
          break;
        }
      }
    }
  }

  return black_count > white_count ? S_BLACK : S_WHITE;
}

static bool GivenGroupLives(const Board* board, short group_idx) {
  const Group* g = &board->_groups[group_idx];
  // At least two liberties.
  if (g->liberties == 1)
    return false;

  // Case 1: Two true eyes.
  Coord eyes[BOARD_SIZE * BOARD_SIZE];
  int eye_count = 0;

  TRAVERSE(
      board, group_idx, c){FOR4(c, _, cc){if (isTrueEye(board, cc, g->color)){
      // Candidate true eyes. Need to double check whether they combined can
      // live.
      bool dup = false;
  for (int i = 0; i < eye_count; ++i) {
    if (eyes[i] == cc) {
      dup = true;
      break;
    }
  }
  if (!dup)
    eyes[eye_count++] = cc;
}
}
ENDFOR4
}
ENDTRAVERSE

if (eye_count <= 1)
  return false;

// Check if there exists at least two eyes, so that each eye is:
// 1. If at the boundary, surrounded by either other candidate true eye, or
// self stones.
// 2. If at the center, surrounded by either other candidate true eye, or self
// stones and at most one enemy stone.
int true_eye_count = 0;
// Coord true_eyes[2];
for (int i = 0; i < eye_count; ++i) {
  int off_board_count = 0;
  int territory_count = 0;

  FORDIAG4(eyes[i], _, cc) {
    Stone s = board->_infos[cc].color;
    if (s == S_OFF_BOARD) {
      off_board_count++;
    } else if (s == S_EMPTY) {
      for (int j = 0; j < eye_count; j++) {
        if (eyes[j] == cc) {
          territory_count++;
          break;
        }
      }
    } else if (s == g->color) {
      territory_count++;
    }
  }
  ENDFORDIAG4
  if ((off_board_count >= 1 && off_board_count + territory_count == 4) ||
      (off_board_count == 0 && off_board_count + territory_count >= 3)) {
    // true_eyes[true_eye_count ++] = eyes[i];
    true_eye_count++;
  }
  if (true_eye_count >= 2) {
    // char buf1[100], buf2[100];
    // printf("True Eyes: %s, %s\n", get_move_str(true_eyes[0], S_EMPTY,
    // buf1), get_move_str(true_eyes[1], S_EMPTY, buf2));
    break;
  }
}

return true_eye_count >= 2 ? true : false;
}

bool GroupInRegion(const Board* board, short group_idx, const Region* r) {
  if (r == nullptr)
    return true;
  bool is_in = false;
  TRAVERSE(board, group_idx, c) {
    if (isIn(r, c)) {
      is_in = true;
      break;
    }
  }
  ENDTRAVERSE

  return is_in;
}

bool OneGroupLives(const Board* board, Stone player, const Region* r) {
  // Check if any of the group lives.
  for (int i = 1; i < board->_num_groups; ++i) {
    if (board->_groups[i].color != player)
      continue;
    // Check if this group is in the region. As long as any of its stones is in
    // the region, we should check it.
    bool is_in = false;
    if (r != nullptr) {
      TRAVERSE(board, i, c) {
        if (isIn(r, c)) {
          is_in = true;
          break;
        }
      }
      ENDTRAVERSE
    } else {
      is_in = true;
    }
    if (is_in && GivenGroupLives(board, i))
      return true;
  }
  return false;
}

#define MOVE_HASH(c, player, ply) (((ply) << 24) + ((player) << 16) + (c))

static inline void update_next_move(Board* board, Coord c, Stone player) {
  board->_next_player = OPPONENT(player);

  board->_last_move4 = board->_last_move3;
  board->_last_move3 = board->_last_move2;
  board->_last_move2 = board->_last_move;
  board->_last_move = c;

  // Compute the hash code with c, player and ply.
  // unsigned long seed = MOVE_HASH(c, player, board->_ply);
  // board->hash ^= fast_random64(&seed);

  board->_ply++;
}

static inline void update_undo(Board* board) {
  // Coord c = board->_last_move;
  board->_last_move = board->_last_move2;
  board->_last_move2 = board->_last_move3;
  board->_last_move3 = board->_last_move4;
  board->_next_player = OPPONENT(board->_next_player);
  board->_ply--;

  // unsigned long seed = MOVE_HASH(c, board->_next_player, board->_ply);
  // board->hash ^= fast_random64(&seed);
}

// Return 0 if there is no ladder, otherwise return the depth of the ladder.
bool find_only_liberty(const Board* b, short id, Coord* m) {
  if (!G_HAS_STONE(id))
    return false;
  if (b->_groups[id].liberties > 1)
    return false;
  TRAVERSE(
      b, id, c){FOR4(c, _, cc){if (b->_infos[cc].color == S_EMPTY){* m = cc;
  return true;
}
}
ENDFOR4
}
ENDTRAVERSE
showBoard(b, SHOW_ALL);
dumpBoard(b);
printf("There must be one liberty for a group [id = %d] with liberty 1.\n", id);
error("");
return false;
}

bool find_two_liberties(const Board* b, short id, Coord m[2]) {
  if (b->_groups[id].liberties != 2)
    return false;
  int counter = 0;
  m[0] = M_PASS;
  m[1] = M_PASS;
  TRAVERSE(b, id, c){FOR4(
      c, _, cc){if (b->_infos[cc].color == S_EMPTY){if (counter == 0){m[0] = cc;
  counter++;
}
else if (counter == 1 && m[0] != cc) {
  m[1] = cc;
  return true;
}
}
}
ENDFOR4
}
ENDTRAVERSE
printf("There must be two liberties for a group with liberty 2");
error("");
return false;
}

bool Play(Board* board, const GroupId4* ids) {
  myassert(board, "Play: Board is nil!");
  myassert(ids, "Play: GroupIds4 is nil!");

  board->_num_group_removed = 0;

  // Place the stone on the coordinate, and update other structures.
  Coord c = ids->c;
  Stone player = ids->player;
  if (c == M_PASS || c == M_RESIGN) {
    update_next_move(board, c, player);
    return isGameEnd(board);
  }

  short new_id = 0;
  unsigned short liberty = ids->liberty;
  short total_capture = 0;
  Coord capture_c = 0;
  bool merge_two_groups_called = false;
  for (int i = 0; i < 4; ++i) {
    // printf("Analysis #%d: id = %d, color = %d, liberty = %d\n", i,
    // ids->ids[i], ids->colors[i], ids->group_liberties[i]);
    // Skip nullptr group.
    if (ids->ids[i] == 0)
      continue;
    unsigned short id = ids->ids[i];
    Group* g = &board->_groups[id];

    Stone s = g->color;
    // The group adjacent to it lose one liberty.
    --g->liberties;

    if (s == player) {
      // Self-group.
      if (new_id == 0) {
        // Merge the current stone with the current group.
        MergeToGroup(board, c, id);
        new_id = id;
        // printf("Merge with group %d, preducing id = %d", id, new_id);
      } else {
        // int prev_new_id = new_id;
        // Merge two large groups.
        new_id = MergeGroups(board, new_id, id);
        merge_two_groups_called = true;
        // printf("Merge with group %d with existing id %d, producing id = %d",
        // id, prev_new_id, new_id);
      }
    } else {
      // Enemy group, If the enemy group has zero liberties, it is killed.
      if (g->liberties == 0) {
        // printf("kill group %d of size %d\n", id, g->stones);
        if (player == S_BLACK)
          board->_b_cap += g->stones;
        else
          board->_w_cap += g->stones;
        total_capture += g->stones;

        // Compute the adjacent enemy point.
        capture_c = c + delta4[i];

        // Add our liberties if the new stone is not yet forming a group.
        // Otherwise the liberties of a dead group's surrounding groups will be
        // taken care of automatically.
        if (new_id == 0) {
          FOR4(c, _, c4) {
            if (board->_infos[c4].id == id)
              liberty++;
          }
          ENDFOR4
        }
        // Remove stones of the group.
        EmptyGroup(board, id);
      }
    }
  }
  // if (new_id > 0) RecomputeGroupLiberties(board, new_id);
  if (merge_two_groups_called)
    RecomputeGroupLiberties(board, new_id);
  if (new_id == 0) {
    // It has not merged with other groups, create a new one.
    set_color(board, c, player);
    // Place the stone.
    board->_infos[c].last_placed = board->_ply;

    new_id = createNewGroup(board, c, liberty);
  }

  // Check simple ko conditions.
  const Group* g = &board->_groups[new_id];
  if (g->liberties == 1 && g->stones == 1 && total_capture == 1) {
    board->_simple_ko = capture_c;
    board->_simple_ko_color = OPPONENT(player);
    board->_ko_age = 0;
  } else {
    board->_ko_age++;
    // board->_simple_ko = M_PASS;
  }

  // We need to run it in the end. After that all group index will be invalid.
  RemoveAllEmptyGroups(board);

  // Finally add the counter.
  update_next_move(board, c, player);
  return false;
}

bool UndoPass(Board* board) {
  if (board->_last_move != M_PASS)
    return false;
  update_undo(board);
  return true;
}

void str_concat(char* buf, int* len, const char* str) {
  *len += sprintf(buf + *len, "%s", str);
}

void showBoard2Buf(const Board* board, ShowChoice choice, char* buf) {
  // Warning [TODO]: possibly buffer overflow.
  char buf2[30];
  int len = 0;
  str_concat(buf, &len, "   ");
  str_concat(buf, &len, BOARD_PROMPT);
  str_concat(buf, &len, "\n");

  char stone[3];
  stone[2] = 0;
  for (int j = BOARD_SIZE - 1; j >= 0; --j) {
    len += sprintf(buf + len, "%2d ", j + 1);
    for (int i = 0; i < BOARD_SIZE; ++i) {
      Coord c = OFFSETXY(i, j);
      Stone s = board->_infos[c].color;
      if (HAS_STONE(s)) {
        if (c == board->_last_move && choice >= SHOW_LAST_MOVE) {
          if (s == S_BLACK)
            strcpy(stone, "X)");
          else
            strcpy(stone, "O)");
        } else {
          if (s == S_BLACK)
            strcpy(stone, "X ");
          else
            strcpy(stone, "O ");
        }
      } else if (s == S_EMPTY) {
        if (STAR_ON(i, j))
          strcpy(stone, "+ ");
        else
          strcpy(stone, ". ");
      } else
        strcpy(stone, "# ");
      str_concat(buf, &len, stone);
    }
    len += sprintf(buf + len, "%d", j + 1);
    if (j == BOARD_SIZE / 2 + 1) {
      len += sprintf(
          buf + len, "     WHITE (O) has captured %d stones", board->_w_cap);
    } else if (j == BOARD_SIZE / 2) {
      len += sprintf(
          buf + len, "     BLACK (X) has captured %d stones", board->_b_cap);
    }
    str_concat(buf, &len, "\n");
  }
  str_concat(buf, &len, "   ");
  str_concat(buf, &len, BOARD_PROMPT);
  if (choice == SHOW_ALL) {
    len += sprintf(buf + len, "\n   #Groups = %d", board->_num_groups - 1);
    len += sprintf(buf + len, "\n   #ply = %d", board->_ply);
    len += sprintf(
        buf + len,
        "\n   Last move = %s",
        get_move_str(board->_last_move, OPPONENT(board->_next_player), buf2));
    len += sprintf(
        buf + len,
        "\n   Last move2 = %s",
        get_move_str(board->_last_move2, board->_next_player, buf2));
    len += sprintf(
        buf + len,
        "\n   Ko point = %s [Age = %d]",
        get_move_str(board->_simple_ko, board->_simple_ko_color, buf2),
        board->_ko_age);
  }
}

void showBoard(const Board* board, ShowChoice choice) {
  // Simple function to show board.
  char buf[2000];
  showBoard2Buf(board, choice, buf);
  // Finally print
  fprintf(stderr, "%s", buf);
}

static int add_title(char* buf) {
  int len = sprintf(buf, "   ");
  len += sprintf(buf, BOARD_PROMPT);
  len += sprintf(buf, "   ");
  return len;
}

static int
add_one_row(const Board* board, int j, ShowChoice choice, char* buf) {
  const char* bg_color_start = "\x1b[1;30;46m";
  const char* bg_color_end = "\x1b[0m";

  const char* fg_black = "\x1b[2;30;46m";
  const char* fg_white = "\x1b[1;37;46m";
  const char* fg_comment = "\x1b[1;30;43m";
  const char* color_last_black = "\x1b[5;30;42m";
  const char* color_last_white = "\x1b[5;37;42m";
  // The unicode for filled circle, but not too big..
  // const char *stone_vis = "\xE2\x97\x8f";
  const char* stone_vis = "@";
  const char* stone_start_vis = "O";

  char stone[30];

  int len = 0;
  len += sprintf(buf + len, "%02d %s", j + 1, bg_color_start);
  for (int i = 0; i < BOARD_SIZE; ++i) {
    Coord c = OFFSETXY(i, j);
    Stone s = board->_infos[c].color;
    if (HAS_STONE(s)) {
      const char* ss_vis = STAR_ON(i, j) ? stone_start_vis : stone_vis;
      if (c == board->_last_move && choice >= SHOW_LAST_MOVE) {
        if (s == S_BLACK)
          sprintf(stone, "%s%s)%s", color_last_black, ss_vis, bg_color_start);
        else
          sprintf(stone, "%s%s)%s", color_last_white, ss_vis, bg_color_start);
      } else {
        if (s == S_BLACK)
          sprintf(stone, "%s%s ", fg_black, ss_vis);
        else
          sprintf(stone, "%s%s ", fg_white, ss_vis);
      }
    } else if (s == S_EMPTY) {
      if (STAR_ON(i, j))
        sprintf(stone, "%s+ ", fg_black);
      else {
        if (choice == SHOW_ROWS) {
          sprintf(stone, "%s%d ", fg_comment, ((j % 10) + 1) % 10);
        } else if (choice == SHOW_COLS) {
          // char c = 'a' + (i >= 8 ? i + 1 : i);
          // sprintf(stone, "%s%d ", fg_comment, ((i % 10) + 1) % 10);
          char c = 'a' + (i >= 8 ? i + 1 : i);
          sprintf(stone, "%s%c ", fg_comment, c);
        } else {
          sprintf(stone, "%s. ", fg_black);
        }
      }
    } else
      sprintf(stone, "%s# ", fg_black);
    str_concat(buf, &len, stone);
  }
  len += sprintf(buf + len, "%s%02d", bg_color_end, j + 1);
  return len;
}

void showBoardFancy(const Board* board, ShowChoice choice) {
  // Simple function to show board. Fancy version.
  char buf[20000];
  char buf2[30];
  int len = 0;
  const char* empty = "      ";

  len += add_title(buf + len);
  if (choice == SHOW_ALL_ROWS_COLS) {
    len += sprintf(buf + len, "%s", empty);
    len += add_title(buf + len);

    len += sprintf(buf + len, "%s", empty);
    len += add_title(buf + len);
  }
  buf[len++] = '\n';

  for (int j = BOARD_SIZE - 1; j >= 0; --j) {
    len += add_one_row(board, j, SHOW_ALL, buf + len);

    if (choice == SHOW_ALL_ROWS_COLS) {
      len += sprintf(buf + len, "%s", empty);

      len += add_one_row(board, j, SHOW_ROWS, buf + len);
      len += sprintf(buf + len, "%s", empty);

      len += add_one_row(board, j, SHOW_COLS, buf + len);
    }

    buf[len++] = '\n';
  }
  len += add_title(buf + len);
  if (choice == SHOW_ALL_ROWS_COLS) {
    len += sprintf(buf + len, "%s", empty);
    len += add_title(buf + len);

    len += sprintf(buf + len, "%s", empty);
    len += add_title(buf + len);
  }
  buf[len++] = '\n';

  len += sprintf(buf + len, "WHITE has captured %d stones\n", board->_w_cap);
  len += sprintf(buf + len, "BLACK has captured %d stones\n", board->_b_cap);

  if (choice >= SHOW_ALL) {
    len += sprintf(buf + len, "\n   #Groups = %d", board->_num_groups - 1);
    len += sprintf(buf + len, "\n   #ply = %d", board->_ply);
    len += sprintf(
        buf + len,
        "\n   Last move = %s",
        get_move_str(board->_last_move, OPPONENT(board->_next_player), buf2));
    len += sprintf(
        buf + len,
        "\n   Last move2 = %s",
        get_move_str(board->_last_move2, board->_next_player, buf2));
    len += sprintf(
        buf + len,
        "\n   Ko point = %s [Age = %d]",
        get_move_str(board->_simple_ko, board->_simple_ko_color, buf2),
        board->_ko_age);
  }
  // Finally print
  fprintf(stderr, "%s", buf);
}

// Debugging
void dumpBoard(const Board* board) {
  char buf[30];
  fprintf(
      stderr,
      "Last move = %s\n",
      get_move_str(board->_last_move, OPPONENT(board->_next_player), buf));
  fprintf(
      stderr,
      "Last move2 = %s\n",
      get_move_str(board->_last_move2, board->_next_player, buf));
  fprintf(stderr, "----Expanded board------------\n");
  ALL_EXPAND_BOARD(board) {
    Stone s = board->_infos[c].color;
    if (s == S_BLACK)
      printf("X ");
    else if (s == S_WHITE)
      printf("O ");
    else if (s == S_EMPTY)
      printf(". ");
    else
      printf("# ");
  }
  END_ALL_EXPAND_BOARD

  // Simple function to show board.
  fprintf(stderr, "----Group ids------------\n");
  ALLBOARD(board) {
    const Info* info = &board->_infos[c];
    if (board->_infos[c].color != S_EMPTY) {
      printf("%03d ", info->id);
    } else {
      printf(" .  ");
    }
  }
  ENDALLBOARD

  fprintf(
      stderr,
      "------Group contents (#groups = %d)-------------\n",
      board->_num_groups - 1);
  for (int i = 1; i < board->_num_groups; ++i) {
    const Group* g = &board->_groups[i];
    fprintf(
        stderr,
        "#%d: color = %d, start = (%d, %d), liberty = %d, stones = %d\n",
        i,
        g->color,
        X(g->start),
        Y(g->start),
        g->liberties,
        g->stones);
    TRAVERSE(board, i, c) {
      const Info* info = &board->_infos[c];
      fprintf(
          stderr,
          "  id = %d, color = %d, coord = (%d, %d), next = (%d, %d)\n",
          info->id,
          board->_infos[c].color,
          X(c),
          Y(c),
          X(info->next),
          Y(info->next));
    }
    ENDTRAVERSE
  }
}

//
void getAllEmptyLocations(const Board* board, AllMoves* all_moves) {
  all_moves->num_moves = 0;
  all_moves->board = board;
  for (int i = 0; i < BOARD_SIZE; ++i) {
    for (int j = 0; j < BOARD_SIZE; ++j) {
      Coord c = OFFSETXY(i, j);
      if (EMPTY(board->_infos[c].color))
        all_moves->moves[all_moves->num_moves++] = c;
    }
  }
}

// Codes used to check the validity of the data structure.
void VerifyBoard(Board* board) {
  // Groups
  // 1. Check if the number of groups is the same as indicated by
  // board->_num_groups.
  unsigned short group_size[MAX_GROUP];
  memset(group_size, 0, sizeof(group_size));

  printf("-----Beging verifying-----\n");
  for (int i = 0; i < BOARD_SIZE; ++i) {
    for (int j = 0; j < BOARD_SIZE; ++j) {
      Coord c = OFFSETXY(i, j);
      const Info* info = &board->_infos[c];
      if (G_HAS_STONE(info->id) != HAS_STONE(board->_infos[c].color)) {
        printf(
            "[VerifyError]: id [%d] and stone [%d] mismatch at (%d, %d)\n",
            info->id,
            board->_infos[c].color,
            X(c),
            Y(c));
      }
      if (HAS_STONE(board->_infos[c].color)) {
        group_size[info->id]++;
      }
    }
  }

  if (board->_num_groups < 1 || board->_num_groups >= MAX_GROUP) {
    printf(
        "[VerifyError]: #groups = %d [MAX_GROUP = %d]",
        board->_num_groups - 1,
        MAX_GROUP);
  }
  // Group id starts from 1.
  for (int i = 1; i < board->_num_groups; ++i) {
    Group* g = &board->_groups[i];
    // printf("Group %d: Start [%d] at (%d, %d)\n", i, g->start, X(g->start),
    // Y(g->start));
    int num_of_stones = 0;

    if (g->color == 0 || g->stones == 0 || g->liberties <= 0 || g->start == 0) {
      printf(
          "[VerifyError]: Group %d is wrong: color: %d, stone: %d [%d from "
          "board], liberties: %d, start: [%d] (%d, %d)\n",
          i,
          g->color,
          g->stones,
          group_size[i],
          g->liberties,
          g->start,
          X(g->start),
          Y(g->start));
      continue;
    }

    TRAVERSE(board, i, c) {
      const Info* info = &board->_infos[c];
      if (info->id != i) {
        printf(
            "[VerifyError]: stone %d:  info->id [%d] and group id [%d] are not "
            "consistent on (%d, %d)\n",
            num_of_stones,
            info->id,
            i,
            X(c),
            Y(c));
      }
      num_of_stones++;
    }
    ENDTRAVERSE
    // Check stones.
    if (num_of_stones != g->stones) {
      printf(
          "[VerifyError]: Group %d: Actual #stones from linked_list [%d] != "
          "recorded [%d]\n",
          i,
          num_of_stones,
          g->stones);
    }
    if (g->stones != group_size[i]) {
      printf(
          "[VerifyError]: Group %d: Actual #stones from board [%d] != recorded "
          "[%d]\n",
          i,
          group_size[i],
          g->stones);
    }
    // Check liberties.
    short recorded_liberties = g->liberties;
    RecomputeGroupLiberties(board, i);
    if (recorded_liberties != g->liberties) {
      printf(
          "[VerifyError]: Group %d: Actual liberty [%d] != recorded [%d]\n",
          i,
          g->liberties,
          recorded_liberties);
      // Bring back the wrong liberties.
      g->liberties = recorded_liberties;
    }

    // Check connectivity.
    Coord* visited = new Coord[group_size[i]];
    ::memset(visited, 0, group_size[i] * sizeof(Coord));
    // A simple BFS with two pointers.
    int push_index = 1, pop_index = 0;
    visited[0] = g->start;
    // EXTREMELY HACK HERE: Use the id as the visited mask.
    board->_infos[g->start].id = 0;

    while (pop_index < push_index) {
      Coord c = visited[pop_index++];
      // printf("Visit (%d, %d)\n", X(c), Y(c));
      if (board->_infos[c].color != g->color) {
        printf(
            "[VerifyError]: Stone at (%d, %d) has different color [%d] than "
            "its group [%d], whose color is %d\n",
            X(c),
            Y(c),
            board->_infos[c].color,
            i,
            g->color);
      }
      FOR4(c, _, c4) {
        if (board->_infos[c4].id == i) {
          // Put it into the queue. Mark it as visited.
          board->_infos[c4].id = 0;
          visited[push_index++] = c4;
        }
      }
      ENDFOR4
    }
    if (push_index != group_size[i]) {
      printf(
          "[VerifyError]: Group %d: connected component has %d entries, while "
          "#group = %d\n",
          i,
          push_index,
          group_size[i]);
    }
    // Resume the id.
    while (--push_index >= 0) {
      board->_infos[visited[push_index]].id = i;
    }
    // Free the memory.
    delete[] visited;
  }
  printf("-----End verifying-----\n");
}

// Compute scores.
bool isEye(const Board* board, Coord c, Stone player) {
  if (board->_infos[c].color != S_EMPTY)
    return false;
  FOR4(c, _, c4) {
    Stone s = board->_infos[c4].color;
    if (s != player && s != S_OFF_BOARD)
      return false;
  }
  ENDFOR4
  return true;
}

// return if an eye is semi-eye (play Coord move to strengthen or falsify)
bool isSemiEye(const Board* board, Coord c, Stone player, Coord* move) {
  *move = M_PASS;
  if (!isEye(board, c, player))
    return false;
  Stone opponent = OPPONENT(player);
  unsigned char num_opponent = 0;
  unsigned char num_boundary = 0;
  unsigned char num_empty = 0;
  FORDIAG4(c, _, c4) {
    Stone s = board->_infos[c4].color;
    if (s == opponent)
      num_opponent++;
    else if (s == S_OFF_BOARD)
      num_boundary++;
    else if (s == S_EMPTY && !isEye(board, c4, player)) {
      num_empty++;
      *move = c4;
    }
  }
  ENDFORDIAG4
  return (num_boundary > 0 && num_opponent == 0 && num_empty == 1) ||
      (num_boundary == 0 && num_opponent == 1 && num_empty == 1);
}

bool isFakeEye(const Board* board, Coord c, Stone player) {
  // enemy count.
  Stone opponent = OPPONENT(player);

  unsigned char num_opponent = 0;
  unsigned char num_boundary = 0;

  FORDIAG4(c, _, c4) {
    Stone s = board->_infos[c4].color;
    if (s == opponent)
      num_opponent++;
    else if (s == S_OFF_BOARD)
      num_boundary++;
  }
  ENDFORDIAG4

  return (
      (num_boundary > 0 && num_opponent >= 1) ||
      (num_boundary == 0 && num_opponent >= 2));
}

bool isTrueEyeXY(const Board* board, int x, int y, Stone player) {
  return isTrueEye(board, OFFSETXY(x, y), player);
}

bool isTrueEye(const Board* board, Coord c, Stone player) {
  return isEye(board, c, player) && !isFakeEye(board, c, player);
}

Stone getEyeColor(const Board* board, Coord c) {
  if (isTrueEye(board, c, S_WHITE))
    return S_WHITE;
  if (isTrueEye(board, c, S_BLACK))
    return S_BLACK;
  return S_EMPTY;
}

float getFastScore(const Board* board, const int rule) {
  short score_black = 0;
  short score_white = 0;
  short stone_black = 0;
  short stone_white = 0;
  for (int i = 0; i < BOARD_SIZE; ++i) {
    for (int j = 0; j < BOARD_SIZE; ++j) {
      Coord c = OFFSETXY(i, j);
      if (board->_infos[c].color == S_BLACK)
        stone_black++;
      else if (board->_infos[c].color == S_WHITE)
        stone_white++;
      else {
        // Empty place, check whether it is an eye.
        Stone eye = getEyeColor(board, c);
        if (eye == S_BLACK)
          score_black++;
        else if (eye == S_WHITE)
          score_white++;
      }
    }
  }
  short cnScore = score_black + stone_black - score_white - stone_white;
  short jpScore = score_black - score_white + board->_b_cap - board->_w_cap -
      board->_rollout_passes;
  if (rule == RULE_JAPANESE)
    return jpScore;
  return cnScore;
}

float getTrompTaylorScore(
    const Board* board,
    const Stone* group_stats,
    Stone* territory) {
  Stone* internal_territory = nullptr;
  if (territory == nullptr) {
    internal_territory = new Stone[BOARD_SIZE * BOARD_SIZE];
    territory = internal_territory;
  }
  ::memset(territory, S_EMPTY, BOARD_SIZE * BOARD_SIZE * sizeof(Stone));

  //
  Coord queue[BOARD_SIZE * BOARD_SIZE];
  int territories[4] = {0, 0, 0, 0};

  // Flood fill, starts with an empty location and expand.
  // The output territory is 1 = BLACK, 2 = WHITE, and 3 = DAME
  for (int i = 0; i < BOARD_SIZE; ++i) {
    for (int j = 0; j < BOARD_SIZE; ++j) {
      Coord c = OFFSETXY(i, j);
      Stone s = board->_infos[c].color;

      // printf("Check %s ... \n", get_move_str(c, s));
      // Skip any stone.
      if (s != S_EMPTY) {
        Stone* t = &territory[EXPORT_OFFSET(c)];
        if (*t == S_EMPTY) {
          // Get the true color of the stone
          unsigned char id = board->_infos[c].id;
          if (group_stats != nullptr && (group_stats[id] & S_DEAD))
            s = OPPONENT(s);
          *t = s;
          territories[s]++;
        }
        continue;
      }

      // It is visited.
      if (territory[EXPORT_OFFSET_XY(i, j)] != S_EMPTY)
        continue;

      // Empty space and it is not visited.
      // Do DFS
      Stone owner = S_EMPTY;
      int q_start = 0, q_end = 0;
      queue[q_end++] = c;
      territory[EXPORT_OFFSET(c)] = S_DAME;

      // printf("Start DFS ...\n");
      // fflush(stdout);
      while (q_end - q_start > 0) {
        Coord cc = queue[q_start++];

        // printf("Visited %s..owner = %d\n", get_move_str(cc,
        // board->_infos[cc].color), owner);
        // fflush(stdout);

        // Put its neighbor into the queue
        FOR4(cc, _, ccc) {
          Stone sss = board->_infos[ccc].color;
          if (sss != S_EMPTY) {
            if (sss != S_OFF_BOARD && owner != S_DAME) {
              Stone* t = &territory[EXPORT_OFFSET(ccc)];
              if (*t == S_EMPTY) {
                // Get the true color of the stone
                unsigned char id = board->_infos[ccc].id;
                if (group_stats != nullptr && (group_stats[id] & S_DEAD))
                  sss = OPPONENT(sss);
                *t = sss;
                territories[sss]++;
              }

              if (owner == S_EMPTY)
                owner = *t;
              else if (owner != *t)
                owner = S_DAME;
              // printf("sss = %d, owner = %d\n", *t, owner);
              // fflush(stdout);
            }
          } else if (territory[EXPORT_OFFSET(ccc)] == S_EMPTY) {
            // Make it visited with S_DAME
            territory[EXPORT_OFFSET(ccc)] = S_DAME;
            // Empty slot and empty territory, put them in.
            queue[q_end++] = ccc;
          }
        }
        ENDFOR4
      }
      // Finish BFS, then we go through the visited again and set them to owner
      // (if owner is not S_DAME).
      if (owner == S_EMPTY) {
        // Empty board.
        return 0.0;
      }
      if (owner != S_DAME) {
        for (int i = 0; i < q_end; ++i) {
          // printf("Deal with %s\n", get_move_str(queue[i], owner));
          // fflush(stdout);
          territory[EXPORT_OFFSET(queue[i])] = owner;
          // Each empty spot only visited once.
          territories[owner]++;
        }
      }
      // Check whether we have visited all board locations, if so, just break.
      if (territories[S_BLACK] + territories[S_WHITE] ==
          BOARD_SIZE * BOARD_SIZE)
        break;
      // fflush(stdout);
    }
  }

  if (internal_territory)
    delete[] internal_territory;

  // Finally count the score.
  float raw_score = territories[S_BLACK] - territories[S_WHITE];
  // std::cout << "black: " << territories[S_BLACK] << ", white: " <<
  // territories[S_WHITE] << ", score: " << raw_score << std::endl;
  return raw_score;
}

bool isGameEnd(const Board* board) {
  return board->_ply > 1 &&
      ((board->_last_move == M_PASS && board->_last_move2 == M_PASS) ||
       board->_last_move == M_RESIGN);
}

// Utilities..Here I assume buf has sufficient space (e.g., >= 30).
char* get_move_str(Coord m, Stone player, char* buf) {
  const char cols[] = "ABCDEFGHJKLMNOPQRST";
  char p = '?';
  switch (player) {
    case S_WHITE:
      p = 'W';
      break;
    case S_BLACK:
      p = 'B';
      break;
    case S_EMPTY:
      p = ' ';
      break;
    case S_OFF_BOARD:
      p = '#';
      break;
  }
  if (m == M_PASS) {
    sprintf(buf, "%c PASS", p);
  } else if (m == M_INVALID) {
    sprintf(buf, "%c INVALID", p);
  } else if (m == M_RESIGN) {
    sprintf(buf, "%c RESIGN", p);
  } else {
    sprintf(buf, "%c %c%d", p, cols[X(m)], Y(m) + 1);
  }
  return buf;
}

void util_show_move(Coord m, Stone player, char* buf) {
  fprintf(
      stderr,
      "Move: x = %d, y = %d, m = %d, str = %s\n",
      X(m),
      Y(m),
      m,
      get_move_str(m, player, buf));
}
