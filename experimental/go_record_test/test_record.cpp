#include "../record.h"
#include "../base/board.h"
#include "../game_feature.h"
#include "../go_state_ext.h"
#include "elf/tree_search/tree_search_base.h"
#include <random>
#include <iostream>

using namespace std;

int main() {
    // Random init.
    mt19937 rng(time(NULL));
    int n_record = 3;

    GameOptions options;
    GoStateExt s(0, options);

    vector<float> first_scores(BOARD_NUM_ACTION);

    for (size_t i = 0; i < n_record; ++i) {
        // Put random moves here.
        Coord c;
        do {
            int x = rng() % BOARD_SIZE;
            int y = rng() % BOARD_SIZE;
            c = getCoord(x, y);
        } while (! s.state().checkMove(c));

        s.forward(c);

        mcts::MCTSPolicy<Coord> mcts_policy;
        vector<float> scores(BOARD_NUM_ACTION);

        float sum = 0;
        for (size_t j = 0; j < BOARD_NUM_ACTION; j++) {
            float score = (rng() % 1000) / 1000.0;
            scores[j] = score;
            sum += score;
        }

        for (size_t j = 0; j < BOARD_NUM_ACTION; j++) {
            mcts_policy.feed(scores[j] / sum, s.state().last_extractor().action2Coord(j));
            if (i == 0) first_scores[j] = scores[j] / sum;
        }

        s.addMCTSPolicy(mcts_policy);
    }

    cout << "----------" << endl;

    string json_str = s.dumpRecord().get_json();

    // Then we can check
    RecordLoader loader;

    Record r = loader.from_json(json_str);
    GoStateExt s2(0, options);
    s2.fromRecord(r);
    s2.switchBeforeMove(0);

    vector<float> mcts_scores(BOARD_NUM_ACTION);

    GoFeature feature(options);
    feature.extractMCTSPi(s2, &mcts_scores[0]);

    for (size_t j = 0; j < BOARD_NUM_ACTION; j++) {
        cout << "first_score = " << first_scores[j] << ", mcts_score = " << mcts_scores[j] << endl;
        if (abs(first_scores[j] - mcts_scores[j]) > 0.01) {
            cerr << "Score doesn't match! " << endl;
            cerr << "first_score = " << first_scores[j] << ", mcts_score = " << mcts_scores[j] << endl;
            return 1;
        }
    }

    return 0;
}
