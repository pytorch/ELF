all:
	${CXX} -std=c++11 -lstdc++ -I../ -I../../ -I../../vendor test_record.cpp ../base/go_state.cc ../sgf/sgf.cc ../base/board.cc ../base/board_feature.cc -o test_record

debug:
	${CXX} -std=c++11 -lstdc++ -I../ -I../../ -I../../vendor test_record.cpp ../base/go_state.cc ../sgf/sgf.cc ../base/board.cc ../base/board_feature.cc -g -fsanitize=address,undefined -o test_record
