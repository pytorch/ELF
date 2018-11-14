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
  float getFinalValue() const { return evaluate(); }
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
    //std::cout << "OTG-DoGoodAction" << std::endl;
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
    return true;
  }
  // helper functions for compatibility with some parts of ELF2, you might ignore this..
  int nextPlayer() const {
    if (_status == 0) return 1; // S_BLACK  FIXME
    if (_status == 1) return 2; // S_WHITE  FIXME dirty hack, I do not use S_WHITE because common.h not included
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
  ActionForChouFleur(int step):Action() { _x[0]=step-2;_x[1]=0;_x[2]=0;_hash=step;} // step is 2 or 3.
  ActionForChouFleur(Action& action):Action(action) {}
  // The action is in 0 0 0 or in 1 0 0.
};


#define StateForChouFleurNumActions 2
#define StateForChouFleurX 1
#define StateForChouFleurY 1
#define StateForChouFleurZ 1
class StateForChouFleur : public State {
 public:
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
 //   std::cout << "OTGChouFleur initialize" << std::endl;
    _xsize[0]=StateForChouFleurX;_xsize[1]=StateForChouFleurY;_xsize[2]=StateForChouFleurZ;  // the features are just one number between 0 and 1 (the distance, normalized).
    _actionSize[0]=2;_actionSize[1]=1;_actionSize[2]=1; // size of the output of the neural network; this should cover the positions of actions (above).
    _hash = DISTANCE; // _hash is an unsigned int, it should be nearly unique.
    _status = 0; // _status is described above, 0 means black plays:
  // 0: black to play.
  // 1: white to play.
  // 2: draw.
  // 3: black has won.
  // 4: white has won.

    // _features is a vector representing the current state. It can (must...) be large for complex games; here just one number between 0 and 1.
    _features.resize(StateForChouFleurX*StateForChouFleurY*StateForChouFleurZ);  // trivial case in dimension 1.
    _features[0] = float(_hash)/DISTANCE; // this is the worst representation I can imagine... brute force would be possible...
    generator.seed(time(NULL));
  // There are two legal actions, 2 and 3.
    if (_actions.size() > 0) return;
    _actions.push_back(new ActionForChouFleur(2));
    _actions[0]->SetIndex(0);
    _actions.push_back(new ActionForChouFleur(3));
    _actions[1]->SetIndex(1);
  } // the hash is the distance.

  // The action just decreases the distance and swaps the turn to play.
  void ApplyAction(const Action& action) {
    if (_hash < action.GetHash()) { _hash = 0; } else { _hash -= action.GetHash();}
	/*if (_hash < 7 || _hash > 95) {
    std::cout << "OTGChouFleur ApplyAction" << _hash << std::endl;
	}*/
    if (_hash <= 0) {
      _status += 3;
    } else {
      _status = 1 - _status;
    }
    _features[0] = float(_hash)/DISTANCE;
  }

  // For this trivial example we just compare to random play. Ok, this is not really a good action.
  // By the way we need a good default DoGoodAction, e.g. one-ply at least. FIXME
  void DoGoodAction() {
    //std::cout << "OTGChouFleur DoGoodAction" << std::endl;
    std::bernoulli_distribution distribution(0.5);
    if (distribution(generator))
      ApplyAction(ActionForChouFleur(2));
    else
      ApplyAction(ActionForChouFleur(3));
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
