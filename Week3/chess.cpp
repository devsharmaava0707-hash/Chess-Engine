#include "chess.hpp"
#include <bits/stdc++.h>
#include"json.hpp"
using namespace std;
using json=nlohmann::json;
struct minmax_result{
    int score;
    chess::Move move;
};
class engine{
    public:
    chess::Movelist legalMoves(chess::Board& board){
        chess::Movelist moves;
        chess::movegen::legalmoves(moves,board);
        return moves;
    }
    bool terminal_history(chess::Board& board,chess::Movelist moves){
        if(moves.empty()) return true;
        else return false;
    }
    int is_win(chess::Board& board,chess::Movelist moves,bool attacker){
        if(moves.empty() && board.inCheck()){
            if(!attacker) return 1;
            else return -1;
        }
        else return 0;
    }
    string toSAN(string fen, chess::Movelist moves){
    chess::Board board(fen);
    string ans;
    for(auto i : moves){
        string san = chess::uci::moveToSan(board, i);
        board.makeMove(i);
        chess::Movelist replies = legalMoves(board);
        if(ans.size()==0) ans = san;
        else ans = ans + " " + san;
    }
    return ans;
   }
    minmax_result minmax(chess::Board &board,int depth,chess::Color attacker,int alpha,int beta){
        chess::Movelist moves=legalMoves(board);
        if(terminal_history(board,moves)){
            int win=is_win(board,moves,board.sideToMove()==attacker);
            if(win==1){
                return {10000+depth,chess::Move{}};
            }
            if(win==-1){
                return {-10000-depth,chess::Move{}};
            }
            return {0,chess::Move{}};
        }
        if(depth<=0){
            return {0,chess::Move{}};
        }
        int bestscore;
        chess::Move bestmove;
        if(board.sideToMove()==attacker){
            bestscore=-30000;
            for(auto &i : moves ){
                board.makeMove(i);
                minmax_result result=minmax(board,depth-1,attacker,alpha,beta);
                // result.score--;
                if(result.score>bestscore){
                    bestscore=result.score;
                    bestmove=i;
                }
                board.unmakeMove(i);
                alpha=max(alpha,bestscore);
                if(alpha>=beta) break;
            }
        }
        else {
             bestscore=30000;
            for(auto &i : moves ){
                board.makeMove(i);
                minmax_result result=minmax(board,depth-1,attacker,alpha,beta);
                // result.score++;
                if(result.score<bestscore){
                    bestscore=result.score;
                    bestmove=i;
                }
                board.unmakeMove(i);
                beta=min(beta,bestscore);
                if(alpha>=beta)break;
            }

        }

        return {bestscore,bestmove};
    }
    chess::Movelist solve(string fen){
        chess::Board board(fen);
        chess::Movelist moves=legalMoves(board);
        chess::Movelist fresult;
        int depth=4;
        chess::Color attacker=board.sideToMove();
        minmax_result result;
        while(!terminal_history(board,moves)){
            result = minmax(board,depth,attacker,-30000,30000);
            fresult.add(result.move);
            board.makeMove(result.move);
            moves = legalMoves(board);
            // cout<<"infinite"<<endl;
        }
        return fresult;
    }
    
};
string normalize(string solution){
    solution = regex_replace(solution, regex("[0-9]+\\.\\s*"), "");
    return solution;
}
int main(){
    cout<<"start of program"<<endl;
    ifstream data("mate_in_2.json");
    json mate2;
    data>>mate2;
    engine functions;
    for(auto& [fen, solution] : mate2.items()){
    chess::Movelist moves = functions.solve(fen);
    string ans = functions.toSAN(fen, moves);
    string expected = normalize(solution.get<string>());
    if(ans==expected) cout<<"true"<<endl;
    else {
        cout<<"...................................."<<endl;
        cout<<"false"<<endl;
        cout<<"expected : "<<expected<<endl;
        cout<<"ans      :"<<ans<<endl;
        cout<<"...................................."<<endl;
    }
}
    // chess::Board board;
    
}