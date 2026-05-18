# Lyra
A strong c++ chess engine.

### [Web Demo](https://hin-yu-evan-fung.github.io/Lyra-UI) - [Source](https://github.com/Hin-Yu-Evan-Fung/Lyra-UI)

### Move generation
* Fancy Magic Bitboards (PEXT)
* Fully legal move generator (Up to 950 Mnps on i5-13400F with bulk counting)

### Move ordering
* Killer move heuristics
* Quiet History
* Continuation History
* Staged move generation
* Static Exchange Evaluation

### Search
* Iterative Deepening
* NegaMax Search
* Quiescence Search
* Transposition Table
* Null Move Pruning
* Late Move Pruning
* Principal variation search
* Futility Pruning
* QSearch Futility Pruning
* Reverse Futility Pruning
* Internal Iterative Deepening
* Singular Extension Search
* History Pruning
* Pawn Correction History

### Evaluation
* PeSTO Evaluation

## Building from source
* Build using g++ 16.1.1
* Haven't tested on Windows or with other compilers yet.

## Acknowledgements
* [Chess Programming Wiki](https://www.chessprogramming.org/Main_Page)
* [OpenBench](https://github.com/AndyGrant/OpenBench)
* [Cutechess](https://cutechess.com/)
* [Chess Programming Channel](https://www.youtube.com/watch?v=QUNP-UjujBM&list=PLmN0neTso3Jxh8ZIylk74JpwfiWNI76Cs) for their amazing tutorials
* Ronald Friederich for the PeSTO Evaluation
* [Stockfish](https://github.com/official-stockfish/Stockfish)
,[Ethereal](https://github.com/AndyGrant/Ethereal) and many other open source chess engines

## License
This project is licensed under the [GNU Affero General Public License v3.0](https://github.com/codedeliveryservice/Reckless/blob/main/LICENSE).
