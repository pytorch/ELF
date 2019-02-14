#ifndef GAME_H__
#define GAME_H__

typedef unsigned short Coord;

#include <execinfo.h>
#include <iostream>
#include <random>
#include <vector>
#include "time.h"
#include <string>
#include <cassert>

#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#define DISTANCE 18   // 17 solved, 20 unsolved

//#include "base/common.h"

/*****
 Action and State are abstract classes.
 Derived classes correspond to various problems.

 A difference with the AZ setting  is that several actions can correspond to the
same logit
 from the neural net. This is useful for complex action spaces in which the list
of possible
 actions is tricky: the MCTS then takes care of differentiating the possible
actions.

 In most of our games, we are still in bijection, but in the case of draughts
this makes a difference.


 We provide an implementation of the ChouFleur game as an example.
 ChouFleur game (usually played with real distance and assuming here that your
foot can decide between length 2 and length 3):
  - we start at distance DISTANCE
  - you remove 2 or 3
  - I remove 2 or 3
  - you remove 2 or 3
  - I remove 2 or 3
  - ...
  - when distance <=0 the player who just removed something wins.
******/

class Action {
 public:
  Action() {_x.resize(3);}
  // Get the location of the move in the neural network output.
  // Several moves might fall in the same location, no pb.
  virtual int GetX() const {return _x[0];}
  virtual int GetY() const {return _x[1];}
  virtual int GetZ() const {return _x[2];}
  unsigned long int GetHash() const { return _hash; }
  void SetIndex(int i) { _i = i; }
  int GetIndex() const { return _i; }

 protected:
  unsigned long int _hash;
  
  // Warning! Two actions might have the same position _x.
  std::vector<int> _x;  // position of the action in {0,...,GetXActionSize()-1} x {0,...,GetYActionSize()-1} x {0,...,GetZActionSize()-1}

  int _i; // index of the action in the list of legal actions in the
  // corresponding state.
  // _i makes sense since an action is never applied to two distinct states. We
  // could have a pointer to
  // the state this action is associated to.
};

class State {
 public:
  State() {_hash=-7;_xsize.resize(3);_actionSize.resize(3);}
  virtual void Initialize() { /*std::cout << " OTG-Initialize" << std::endl;*/ exit(-1); }
  virtual void ApplyAction(__attribute__((unused)) const Action& action) { /*std::cout << "OTG-ApplyAction" << std::endl;*/ exit(-1); }
  virtual const std::vector<Action*>& GetLegalActions() const { return _actions; }
  bool checkMove(const unsigned short& c) const { return c < _actions.size(); }
  unsigned long int GetHash() const { return _hash; }
  float evaluate() const {
	fprintf(stderr, "evaluate");
    if (_status == 3) {std::cout << "blackwon" << std::endl;}
    if (_status == 4) {std::cout << "whitewon" << std::endl;}
    if (_status == 3) {return 1;}
    if (_status == 4) {return -1.;}
     return 0.;}
  // Returns info about the current state.
  // 0: black to play.
  // 1: white to play.
  // 2: draw.
  // 3: black has won.
  // 4: white has won.
  int GetStatus() const {
  return _status; };
  bool terminated() const {
  return _status > 1; };
  float getFinalValue() const {
	fprintf(stderr, "BTgetfinalvalue");
  return evaluate(); }
  // Returns a pointer to GetXSize x GetYSize x GetZSize float, input for the
  // NN.
  const float* GetFeatures() const { return &_features[0]; }
  virtual const std::string& showBoard() const { return boardstring; }
  // Size of the neural network input.
  int GetXSize() const { return _xsize[0]; }
  int GetYSize() const { return _xsize[1]; }
  int GetZSize() const { return _xsize[2]; }
  // Size of the neural network output.
  int GetXActionSize() const {return _actionSize[0];}
  int GetYActionSize() const {return _actionSize[1];}
  int GetZActionSize() const {return _actionSize[2];}
  virtual void DoGoodAction() {
    std::cout << "OTG-BT-DoGoodAction" << std::endl;
    exit(-1);
  }
  bool moves_since(size_t* next_move_number, std::vector<Coord>* moves) const {
    //std::cout << "OTG-moves_since" << std::endl;
    if (*next_move_number > _moves.size()) {
      // The move number is not right.
      return false;
    }
    moves->clear();
    for (size_t i = *next_move_number; i < _moves.size(); ++i) {
      moves->push_back(_moves[i]);
    }
    *next_move_number = _moves.size();
    std::cout << "OTG-BT-nmn" << _moves.size() << std::endl;
    return true;
  }
  // helper functions for compatibility with some parts of ELF2, you might ignore this..
  int nextPlayer() const {
    if (_status == 0) return 1; // S_BLACK  FIXME
    if (_status == 1) return 0; // S_WHITE  FIXME dirty hack, I do not use S_WHITE because common.h not included
    //std::cout << "OTG-nextplayer crashes " << _status << std::endl;
    return 0;
  }
  virtual int getPly() const { return -1; }
  bool forward(const Action& action) {
    //std::cout << "OTG-forwarda" << action.GetHash() << std::endl;
    // maybe we should check legality here in bool forward(Coord) ? FIXME
    ApplyAction(action);
	 /* void *array[10];
	    size_t size;

		  // get void*'s for all entries on the stack
		    size = backtrace(array, 10);

			  // print out all the frames to stderr
	//		    fprintf(stderr, "Error: signal %d:\n", sig);
				  backtrace_symbols_fd(array, size, STDERR_FILENO);

	assert(0);
	*/
    return true;  // FIXME forward always return true ?
  }
  bool forward(const unsigned short& coord) {
    //std::cout << "OTG-forwardb" << coord << std::endl;
    _moves.push_back(coord);
    return forward(*GetLegalActions()[coord]);
  }
  void reset() { // for backward compatibility, don't worry :-)
    _moves.clear();
    Initialize();
  }
  int _GetStatus() const { // for compatibility, don't worry...
    //std::cout << "OTG-_getStatus" << std::endl;
    return GetStatus();
  }
  // end of the helper functions that you might ignore.

  bool justStarted() const {
    return _moves.size() == 0;
  }

 protected:
  std::vector<float>_features; // neural network input
  std::vector<Action*> _actions;
  int _status;
  std::vector<int> _xsize; // size of the neural network input
  std::vector<int> _actionSize; // size of the neural network output
  unsigned long int _hash;
  std::vector<Coord> _moves;
  std::string boardstring = "";  // string for visualization.
  // corresponding state.
  // _i makes sense since an action is never applied to two distinct states. We
  // could have a pointer to
  // the state this action is associated to.
};

// let us implement the ChouFleur game.
// Players start at distance 100, and each move reduces the distance by 2 or 3;
// if the distance
// is not positive after your turn then you win.
class StateForChouFleur;
class ActionForChouFleur : public Action {
  friend StateForChouFleur;

 public:
  ActionForChouFleur():Action() {}
  // each action has a position (_x[0], _x[1], _x[2])
  // here for ChouFleur, there is (0, 0, 0) and (1, 0, 0), corresponding to steps 2 and 3 respectively.
  //ActionForChouFleur(int step):Action() { _x[0]=step-2;_x[1]=0;_x[2]=0;_hash=step;} // step is 2 or 3.
 ActionForChouFleur(int x, int y, int direction):Action() { _x[0]=x;_x[1]=y;_x[2]=direction;_hash= (x + y * 8) * 3 + direction;} // step i
 ActionForChouFleur(Action& action):Action(action) {}
  // The action is in 0 0 0 or in 1 0 0.
};

const int White = 0;
const int Black = 1;
const int Empty = 2;

const int Dx = 8;
const int Dy = 8;

const int MaxLegalMoves = 3 * Dx * 2;
const int MaxPlayoutLength = 1000;

const int MaxMoveNumber = 80 * 2 * 2 * (3 * Dx * Dy) + 1;

const unsigned long long HashArray [2] [Dx] [Dy] = {{{16169835459508955805ULL,8687539127158664826ULL,17108160493861513873ULL,14866772039050829819ULL,6382425218817137013ULL,14755416853480619523ULL,262222823330570628ULL,13725127845745622661ULL},{3636680052915828731ULL,16420612890348964839ULL,6860860612143234356ULL,18319421942982323461ULL,7517309626840007552ULL,10232127189939683483ULL,955717582448734283ULL,8667483786766950674ULL},{2140948988612357176ULL,13169326505115950933ULL,9043089259473408367ULL,14996796748455957877ULL,15690658049786921672ULL,14036214656995497049ULL,8074374651246879349ULL,7438200393285415060ULL},{11187005889026272334ULL,12949944768259122662ULL,15040907901836627032ULL,12763962205942898831ULL,16979172226678533126ULL,4767380695075439596ULL,11357595680782992511ULL,18317812704978456668ULL},{14917024458661891820ULL,7153012356196806730ULL,7046853297116196052ULL,13589609198741071495ULL,6790311378931853624ULL,16395541026852423248ULL,11689928350345550686ULL,11066307942870238330ULL},{166037065768007008ULL,11418603408886607759ULL,12495334242248696588ULL,2313557874151223826ULL,14577118380010101296ULL,15648089824920020193ULL,4971894234579520583ULL,11057316417677697807ULL},{8207667517007284943ULL,8523579834061669855ULL,8940007351769745059ULL,10968608755118922263ULL,1559018261319330017ULL,14782379423393254003ULL,14762225579280523571ULL,10944216625444453360ULL},{3736596993699687090ULL,9537226116780554838ULL,6137756171499266159ULL,15518337951594466340ULL,8949482491910899126ULL,12000694956930405168ULL,17197495493249131043ULL,12035377088540875303ULL}},{{11072182272252713633ULL,18010027927339653559ULL,5566153019321090447ULL,12179673189384747138ULL,13568320540822843442ULL,11695876895037360369ULL,6786324505691748745ULL,11971963882150787078ULL},{3292727085030657134ULL,16040269400726833725ULL,205205274671649419ULL,5414198750221716225ULL,14822232492594364459ULL,8087037031988850037ULL,7074371490665697308ULL,11201942530787631417ULL},{15302063331589353095ULL,10139618060405873107ULL,10885973555582100003ULL,700768787738394253ULL,11436380602037827173ULL,3962337549481533006ULL,7610480833667588795ULL,2068365834880889651ULL},{14479802558981603625ULL,4775538863591284692ULL,11861314192536164614ULL,5571373744305498377ULL,16087187627008913836ULL,12690383218059263823ULL,13938852539599657576ULL,11534571120884325585ULL},{6103925850415086531ULL,16583445213172046727ULL,11055383655933654569ULL,11036500694084901452ULL,1216696450232701521ULL,15355059884847912069ULL,6948082711389627959ULL,13186654485806700998ULL},{1223503520381324471ULL,3202032567972352774ULL,14182058217600591584ULL,16345366887391355636ULL,7938496525083787096ULL,6733969192016572985ULL,1207955508137446886ULL,4689090394441190407ULL},{13051004392307142196ULL,15419649645945058733ULL,8999056803418768900ULL,13866985612211194599ULL,17241193493089699198ULL,6642778445250924916ULL,4214693391800225934ULL,1785700724540658093ULL},{3779965625422541689ULL,18198938396262131434ULL,13755289309211499846ULL,10967706938447926321ULL,10351157047240713263ULL,15428463797457287716ULL,5518344826184209350ULL,15376329534511616666ULL}}};
const unsigned long long HashTurn = 7558115252641239713ULL;

class Move {
 public :
  int x, y, x1, y1, color;
  int code;
  
  int numberPrevious () {
    int c = 0;
    if (color == White)
      return c + 3 * (x + Dx * y) + x1 - x + 1;
    else
      return c + 3 * Dx * Dy + 3 * (x + Dx * y) + x1 - x + 1;
  }

  int number () {
    int c = 0;
    if (color == White)
      return c + 3 * (x + Dx * y) + x1 - x + 1;
    else
      return c + 3 * Dx * Dy + 3 * (x + Dx * y) + x1 - x + 1;
  }
};

#define StateForChouFleurNumActions 64 * 3
#define StateForChouFleurX 2
#define StateForChouFleurY 8
#define StateForChouFleurZ 8

class StateForChouFleur : public State {
 public:
  int board [Dx] [Dy];
  unsigned long long hash;
  Move rollout [MaxPlayoutLength];
  int length, turn;

  void init () {
	fprintf(stderr, "BT INIT");
    for (int i = 0; i < Dx; i++)
      for (int j = 0; j < Dy; j++)
        board [i] [j] = Empty;
    for (int i = 0; i < 2; i++)
      for (int j = 0; j < Dx; j++)
        board [j] [i] = Black;
    for (int i = Dy - 2; i < Dy; i++)
      for (int j = 0; j < Dx; j++)
	board [j] [i] = White;
    hash = 0;
    length = 0;
    turn = White;
    //initHash ();
  }

  bool won (int color) {
    if (color == White) {
      for (int j = 0; j < Dx; j++)
        if (board [j] [0] == White)
          return true;
      Move listeCoups [MaxLegalMoves];
      int nb = legalMoves (Black, listeCoups);
      if (nb == 0)
	return true;
    }
    else {
      for (int j = 0; j < Dx; j++)
        if (board [j] [Dy - 1] == Black)
          return true;
      Move listeCoups [MaxLegalMoves];
      int nb = legalMoves (White, listeCoups);
      if (nb == 0)
	return true;
    }
    return false;
  }

  bool terminal () {
    //return won (Black) || won (White);                                                                                                      
    for (int j = 0; j < Dx; j++)
      if (board [j] [0] == White)
        return true;
    for (int j = 0; j < Dx; j++)
      if (board [j] [Dy - 1] == Black)
        return true;
    Move listeCoups [MaxLegalMoves];
    int nb = legalMoves (turn, listeCoups);
    if (nb == 0)
        return true;
    return false;
  }

  int score () {
    if (won (White))
      return 1;
    return 0;
  }

  float evaluation (int color) {
	fprintf(stderr, "eval");
    if (won (color))
      return 1000000.0;
    if (won (opponent (color)))
      return -1000000.0;
    Move moves [MaxLegalMoves];
    int nb = legalMoves (turn, moves);
    if (nb == 0) {
      if (color == turn)
        return -1000000.0;
      else
        return 1000000.0;
    }
    int nbOpponent = legalMoves (opponent (turn), moves);
    if (color == turn)
      return (float)(nb - nbOpponent);
    return (float)(nbOpponent - nb);
  }

  int opponent (int joueur) {
    if (joueur == White)
      return Black;
    return White;
  }

  bool legalMove (Move m) {
    if (board [m.x] [m.y] != m.color)
      return false;
    if (board [m.x1] [m.y1] == m.color)
      return false;
    if (m.color == White)
      if ((m.y1 == m.y - 1) && (m.x == m.x1))
        if (board [m.x1] [m.y1] == Black)
          return false;
    if (m.color == Black)
      if ((m.y1 == m.y + 1) && (m.x == m.x1))
        if (board [m.x1] [m.y1] == White)
          return false;
     return true;
  }

  void play (Move m) {
	fprintf(stderr, "play");
    board [m.x] [m.y] = Empty;
    hash ^= HashArray [m.color] [m.x] [m.y];
    if (board [m.x1] [m.y1] != Empty)
      hash ^= HashArray [board [m.x1] [m.y1]] [m.x1] [m.y1];
    board [m.x1] [m.y1] = m.color;
    hash ^= HashArray [m.color] [m.x1] [m.y1];
    hash ^= HashTurn;
    if (length < MaxPlayoutLength) {
      rollout [length] = m;
      length++;
    }
    else
      fprintf (stderr, "Pb play,");
    turn = opponent (turn);
  }

  int legalMoves (int color, Move moves [MaxLegalMoves]) {
    int nb = 0;
    for (int i = 0; i < Dx; i++)
      for (int j = 0; j < Dy; j++)
        if (board [i] [j] == color) {
          Move m;
          m.x = i;
          m.y = j;
          m.color = color;
          if (color == White) {
            if ((j - 1 >= 0) && (i + 1 < Dx)) {
              m.x1 = i + 1;
              m.y1 = j - 1;
              if (board [m.x1] [m.y1] == Empty)
                m.code = 0;
              else
                m.code = 6 * Dx * Dy;
              if (legalMove (m)) {
                moves [nb] = m;
                nb++;
              }
            }
            if ((j - 1 >= 0) && (i - 1 >= 0)) {
              m.x1 = i - 1;
              m.y1 = j - 1;
              if (board [m.x1] [m.y1] == Empty)
                m.code = 0;
              else
                m.code = 6 * Dx * Dy;
              if (legalMove (m)) {
                moves [nb] = m;
                nb++;
              }
            }
            if ((j - 1 >= 0)) {
              m.x1 = i;
              m.y1 = j - 1;
              if (board [m.x1] [m.y1] == Empty)
                m.code = 0;
              else
                m.code = 6 * Dx * Dy;
              if (legalMove (m)) {
                moves [nb] = m;
                nb++;
              }
            }
          }
          else {
            if ((j + 1 < Dy) && (i + 1 < Dx)) {
              m.x1 = i + 1;
              m.y1 = j + 1;
              if (board [m.x1] [m.y1] == Empty)
                m.code = 0;
              else
                m.code = 6 * Dx * Dy;
              if (legalMove (m)) {
                moves [nb] = m;
                nb++;
              }
            }
            if ((j + 1 < Dy) && (i - 1 >= 0)) {
              m.x1 = i - 1;
              m.y1 = j + 1;
              if (board [m.x1] [m.y1] == Empty)
                m.code = 0;
              else
                m.code = 6 * Dx * Dy;
              if (legalMove (m)) {
                moves [nb] = m;
                nb++;
              }
	    }
	    if ((j + 1 < Dy)) {
	      m.x1 = i;
	      m.y1 = j + 1;
              if (board [m.x1] [m.y1] == Empty)
                m.code = 0;
              else
                m.code = 6 * Dx * Dy;
              if (legalMove (m)) {
                moves [nb] = m;
                nb++;
              }
            }
          }
        }
    return nb;
  }
  
 StateForChouFleur():State() {
  //  std::cout << "OTGChouFleur CreateState" << std::endl;
    Initialize();
  //  std::cout << "OTGChouFleur CreateState done" << std::endl;
  }
  virtual ~StateForChouFleur() {
//    std::cout << "OTGChouFleur DeleteState done" << std::endl;
  }
  // We start at distance 100, black plays first.
  void Initialize() {
    // People implementing classes should not have much to do in _moves; just _moves.clear().
    _moves.clear();
	fprintf(stderr, "BTinitialize");
 //   std::cout << "OTGChouFleur initialize" << std::endl;
    _xsize[0]=StateForChouFleurX;_xsize[1]=StateForChouFleurY;_xsize[2]=StateForChouFleurZ;  // the features are just one number between 0 and 1 (the distance, normalized).                                                                                                      
    _actionSize[0]=8;_actionSize[1]=8;_actionSize[2]=3; // size of the output of the neural network; this should cover the positions of actions (above).                                                                                                                                   
    _hash = 0; // _hash is an unsigned int, it should be nearly unique.                                                                       
    _status = 1; // _status is described above, 0 means black plays:                                                                          
  // 0: black to play.
  // 1: white to play.
  // 2: draw.
  // 3: black has won.
  // 4: white has won.

    // _features is a vector representing the current state. It can (must...) be large for complex games; here just one number between 0 and 1.

    /*
      _features.resize(StateForChouFleurX*StateForChouFleurY*StateForChouFleurZ);  // trivial case in dimension 1.
//    _features[0] = float(_hash)/DISTANCE; // this is the worst representation I can imagine... brute force would be possible...
	for (int i=0;i<DISTANCE+1;i++) {
      _features[i] = (float(_hash) < float(i)) ? 1. : 0.;
	}
    */
    _features.resize(StateForChouFleurX*StateForChouFleurY*StateForChouFleurZ);  // trivial case in dimension 1.                     
    generator.seed(time(NULL));
    init ();
    findFeatures ();
    if (_actions.size() > 0) {
    std::cout << "OTGBreakthrough initendmid" << std::endl;
	return;
    }findActions (White);
    std::cout << "OTGBreakthrough initend" << std::endl;
  }

  void findActions (int color) {
    std::cout << "OTGBreakthrough findActions " << std::endl;
    Move moves [MaxLegalMoves];
    int nb = legalMoves (color, moves);

    _actions.clear ();
    for (int i = 0; i < nb; i++) {
      int x = moves [i].x;
      int y = moves [i].y;
      int dir = 2;
      if (moves [i].x1 == x - 1)
        dir = 0;
      else if (moves [i].x1 == x)
        dir = 1;
      _actions.push_back(new ActionForChouFleur(x, y, dir));
      _actions[i]->SetIndex(i);
    }
    std::cout << "OTGBreakthrough findActions end " << std::endl;
  }

  void findFeatures () {
    for (int i = 0; i < 128; i++)
      _features [i] = 0;
    for (int i = 0; i < 64; i++)
      if (board [i % 8] [i / 8] == Black)
        _features [i] = 1;
    for (int i = 0; i < 64; i++)
      if (board [i % 8] [i / 8] == White)
        _features [64 + i] = 1;
  }

  void ApplyAction(const Action& action) {
    std::cout << "OTGBreakthrough ApplyAction " << std::endl;
    Move m;
	fprintf(stderr, "BTapplyaction");
    if (_status == 1) { // White                                                                                                             
	  fprintf(stderr, "BTwhite");
      m.color = White;
      m.x = action.GetX ();
      m.y = action.GetY ();
      if (action.GetZ () == 0) {
        m.x1 = action.GetX () - 1;
        m.y1 = action.GetY () - 1;
      }
      else if (action.GetZ () == 1) {
        m.x1 = action.GetX ();
        m.y1 = action.GetY () - 1;
      }
      else if (action.GetZ () == 2) {
        m.x1 = action.GetX () + 1;
        m.y1 = action.GetY () - 1;
      }
      play (m);
      findActions (Black);
      if (won (White))
        _status = 4;
      else
        _status = 0;
    }
    else { // Black                                                                                                                          
	  fprintf(stderr, "BTBlack");
      m.color = Black;
      m.x = action.GetX ();
      m.y = action.GetY ();
      if (action.GetZ () == 0) {
        m.x1 = action.GetX () - 1;
        m.y1 = action.GetY () + 1;
      }
      else if (action.GetZ () == 1) {
        m.x1 = action.GetX ();
        m.y1 = action.GetY () + 1;
      }
      else if (action.GetZ () == 2) {
        m.x1 = action.GetX () + 1;
        m.y1 = action.GetY () + 1;
      }
      play (m);
      findActions (White);
      if (won (Black))
        _status = 3;
      else
        _status = 1;
    }
    findFeatures ();
    _hash = hash;
    std::cout << "OTGBreakthrough ApplyAction end " << std::endl;
  }
  
  // For this trivial example we just compare to random play. Ok, this is not really a good action.
  // By the way we need a good default DoGoodAction, e.g. one-ply at least. FIXME
  void DoGoodAction() {
    std::cout << "OTGBreakthrough DoGoodAction" << std::endl;

    int i = rand () % _actions.size ();
    ActionForChouFleur a = *_actions [i];
    ApplyAction(a);
    std::cout << "OTGBreakthrough DoGoodAction end" << std::endl;
  }

 protected:
  std::default_random_engine generator;
};


// let us implement Draughts. This is a class compatible with Elf2, to be filled
// so that we can play with the AI.
// I assume below that the AI has a class for Board and a class for Actions. An
// action is an "entire" action, including the several jumps that an action can
// include.
class StateForDraughts;
class ActionForDraughts : public Action {
  friend StateForDraughts;
  // for black:
  // _x[0]; // between 0 and 9, x-axis of the piece to be moved.
  // _x[1]; // between 0 and 4, y-axis of the piece to be moved, divided by 2 (rounded below).
  // _x[2]; // 0 if moving to the left, 1 if moving to the right (from the player's point of view on the board).

  // for white: symetry w.r.t the center of the board!
  // i.e. for black _x = xaxis    and   _y = yaxis // 2
  //  and for white _x = 9-xaxis  and   _y = (9-yaxis) // 2

  // We also inherit _status, which tells us if it's black's turn or white's turn
  // to play. Reminder:
  // 0: black to play.
  // 1: white to play.
  // 2: draw.
  // 3: black has won.
  // 4: white has won.

 public:
  ActionForDraughts():Action() {}
  ActionForDraughts(Action& action):Action(action) {}

 protected:
  // TODO here we should have an object of the the AI class for actions.

};

#define StateForDraughgtsNumActions 37
class StateForDraughts : public State {
 public:
  StateForDraughts():State() {
    _xsize[0] = 10; _xsize[1] = 5; _xsize[2] = 4; // size of the neural net input.
    // the neural network specifies which piece should be moved and if it should start by left or by right:
    _actionSize[0]=10;_actionSize[1]=5;_actionSize[2]=2; // size of the neural net output; last if left(0)/right(1).
    Initialize();
  }

  // This function initializes the board at its original state.
  void Initialize() {
    // TODO draughts this should initialize the the AI board.
    _status = 1; // because at draughts White plays first.
    _features.resize(200); // 50 locations, and 4
    _moves.clear();
    // boards because 4
    // different types of pieces
    // (black, white, black
    // king, white king).

    // TODO here we should use the AI's implementation of Board for:
    // 1. generating the list of legal actions.
    // 2. storing them in our this->_actions with the correct i_ for each
    //    (i.e. _actions[i]._i = i, this is just indexing).
  }

  // The action just decreases the distance and swaps the turn to play.
  void ApplyAction(__attribute__((unused)) const Action& action) {
    // TODO draughts: we should update:
    // 1. the the AI board object.
    // 2. the _features vector: for black:
    //  _features[i] should be 1 if square i is black
    //  _features[50+i] should be 1 if square i is white
    //  _features[100+i] should be 1 if square i is black king
    //  _features[150+i] should be 1 if square i is white king
    // 2. the _features vector: for white: 
    //  _features[i] should be 1 if square 49-i is white 
    //  _features[50+i] should be 1 if square 49-i is black
    //  _features[100+i] should be 1 if square 49-i is white king
    //  _features[150+i] should be 1 if square 49-i is black king
  // We will take care to design data augmentation techniques for having
  // several channels; here there is only 4 channels,
  // so that people who use Elf2 do not have to understand anything from Elf2.
  // For example, channels could include the history.
    //
    // We should also update _status if one of the players wins or if it's a
    // draw.
    _status = 1 - _status;  // if nobody wins...
  }

  // The feature space has dimension 1x1x1.
  // This is just the distance, i.e. the _hash.

  // For this trivial example we just compare to random play.
  void DoGoodAction() {
    // TODO this should request a good action as suggest by the AI and apply
    // this->ApplyAction accordingly.
    // This will not be used during the learning, but for checking that the
    // learning provides
    // an improvement that can decently be included in the AI.
  }

};

#endif
