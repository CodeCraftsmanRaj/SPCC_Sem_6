#include <iostream>
#include <stack>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <iomanip>

using namespace std;

set<int> followpos[100]; 
map<int, char> posToSymbol; 
int leafNodeCount = 0;   

struct Node {
    char value;         
    int position;       
    Node *left, *right; 
    bool nullable;
    set<int> firstpos;
    set<int> lastpos;

    Node(char val) : value(val), position(0), left(nullptr), right(nullptr), nullable(false) {}
};

bool isOperator(char c) {
    return c == '*' || c == '|' || c == '.';
}

bool isOperand(char c) {
    return !isOperator(c) && c != '(' && c != ')';
}

string addConcatSymbol(string regex) {
    string res = "";
    for (int i = 0; i < regex.length(); i++) {
        char c1 = regex[i];
        res += c1;
        if (i + 1 < regex.length()) {
            char c2 = regex[i + 1];
            bool c1IsOp = isOperand(c1);
            bool c2IsOp = isOperand(c2);
            
            if ((c1IsOp && c2IsOp) || 
                (c1IsOp && c2 == '(') || 
                (c1 == '*' && c2IsOp) || 
                (c1 == '*' && c2 == '(') || 
                (c1 == ')' && c2IsOp) || 
                (c1 == ')' && c2 == '(')) {
                res += '.';
            }
        }
    }
    return res;
}

int precedence(char c) {
    if (c == '*') return 3;
    if (c == '.') return 2;
    if (c == '|') return 1;
    return 0;
}

string infixToPostfix(string regex) {
    string postfix = "";
    stack<char> s;
    for (char c : regex) {
        if (isOperand(c)) {
            postfix += c;
        } else if (c == '(') {
            s.push(c);
        } else if (c == ')') {
            while (!s.empty() && s.top() != '(') {
                postfix += s.top();
                s.pop();
            }
            s.pop();
        } else {
            while (!s.empty() && precedence(s.top()) >= precedence(c)) {
                postfix += s.top();
                s.pop();
            }
            s.push(c);
        }
    }
    while (!s.empty()) {
        postfix += s.top();
        s.pop();
    }
    return postfix;
}

Node* buildTree(string postfix) {
    stack<Node*> st;
    leafNodeCount = 0;
    
    for (char c : postfix) {
        if (isOperand(c)) {
            Node* node = new Node(c);
            node->position = ++leafNodeCount;
            posToSymbol[node->position] = c;
            
            node->nullable = false;
            node->firstpos.insert(node->position);
            node->lastpos.insert(node->position);
            st.push(node);
        } else if (c == '*') { 
            Node* node = new Node(c);
            node->left = st.top(); st.pop();
            
            node->nullable = true;
            node->firstpos = node->left->firstpos;
            node->lastpos = node->left->lastpos;
            st.push(node);
        } else { 
            Node* node = new Node(c);
            node->right = st.top(); st.pop();
            node->left = st.top(); st.pop();
            
            if (c == '|') {
                node->nullable = node->left->nullable || node->right->nullable;
                
                node->firstpos = node->left->firstpos;
                node->firstpos.insert(node->right->firstpos.begin(), node->right->firstpos.end());
                
                node->lastpos = node->left->lastpos;
                node->lastpos.insert(node->right->lastpos.begin(), node->right->lastpos.end());
            } else if (c == '.') {
                node->nullable = node->left->nullable && node->right->nullable;
                
                if (node->left->nullable) {
                    node->firstpos = node->left->firstpos;
                    node->firstpos.insert(node->right->firstpos.begin(), node->right->firstpos.end());
                } else {
                    node->firstpos = node->left->firstpos;
                }
                
                if (node->right->nullable) {
                    node->lastpos = node->left->lastpos;
                    node->lastpos.insert(node->right->lastpos.begin(), node->right->lastpos.end());
                } else {
                    node->lastpos = node->right->lastpos;
                }
            }
            st.push(node);
        }
    }
    return st.top();
}

void computeFollowpos(Node* node) {
    if (!node) return;
    
    if (node->value == '.') {
        for (int i : node->left->lastpos) {
            followpos[i].insert(node->right->firstpos.begin(), node->right->firstpos.end());
        }
    }
    else if (node->value == '*') {
        for (int i : node->lastpos) {
            followpos[i].insert(node->firstpos.begin(), node->firstpos.end());
        }
    }
    
    computeFollowpos(node->left);
    computeFollowpos(node->right);
}

void printSet(set<int> s) {
    cout << "{ ";
    for (int i : s) cout << i << " ";
    cout << "}";
}

void printTreeDetails(Node* node) {
    if (!node) return;
    printTreeDetails(node->left);
    printTreeDetails(node->right);
    
    cout << "Node: " << node->value;
    if(isOperand(node->value)) cout << " (" << node->position << ")";
    cout << "\t| Nullable: " << (node->nullable ? "true" : "false");
    cout << "\t| Firstpos: "; printSet(node->firstpos);
    cout << "\t| Lastpos: "; printSet(node->lastpos);
    cout << endl;
}

void printPrettyTree(Node* root, int space = 0) {
    const int COUNT = 10;
    if (root == nullptr) return;
    
    space += COUNT;
    
    printPrettyTree(root->right, space);
    
    cout << endl;
    for (int i = COUNT; i < space; i++) cout << " ";
    cout << "[" << root->value;
    if (isOperand(root->value)) cout << ":" << root->position;
    cout << "]" << "\n";
    
    printPrettyTree(root->left, space);
}

void buildDFA(Node* root, vector<char> alphabet) {
    map<set<int>, int> stateMapping;
    vector<set<int>> Dstates;
    vector<vector<int>> transitionTable;
    
    set<int> startState = root->firstpos;
    Dstates.push_back(startState);
    stateMapping[startState] = 0;
    
    int processedCount = 0;
    
    while (processedCount < Dstates.size()) {
        set<int> currentSet = Dstates[processedCount];
        int currentStateID = processedCount;
        processedCount++;
        
        vector<int> row; 
        
        for (char sym : alphabet) {
            set<int> newSet;
            
            for (int p : currentSet) {
                if (posToSymbol[p] == sym) {
                    newSet.insert(followpos[p].begin(), followpos[p].end());
                }
            }
            
            if (newSet.empty()) {
                row.push_back(-1); 
            } else {
                if (stateMapping.find(newSet) == stateMapping.end()) {
                    stateMapping[newSet] = Dstates.size();
                    Dstates.push_back(newSet);
                }
                row.push_back(stateMapping[newSet]);
            }
        }
        transitionTable.push_back(row);
    }
    
    cout << "\n\n=== DFA State Transition Table ===\n\n";
    
    cout << setw(10) << "State" << " | ";
    for (char c : alphabet) {
        cout << setw(10) << c << " | ";
    }
    cout << "\n" << string(10 + (alphabet.size() * 13), '_') << "\n";
    
    for (int i = 0; i < transitionTable.size(); i++) {
        bool isFinal = false;
        for (int p : Dstates[i]) {
            if (posToSymbol[p] == '#') {
                isFinal = true;
                break;
            }
        }
        
        string stateLabel = (isFinal ? "*" : "") + to_string(i);
        cout << setw(10) << stateLabel << " | ";
        
        for (int nextState : transitionTable[i]) {
            if (nextState == -1) cout << setw(10) << "-" << " | ";
            else cout << setw(10) << nextState << " | ";
        }
        cout << "\n";
    }
    cout << "\n(* denotes Accepting State)\n";
}

int main() {
    string regex;
    cout << "Enter Regular Expression: ";
    cin >> regex;
    
    regex = "(" + regex + ")#"; 
    
    string explicitRegex = addConcatSymbol(regex);
    string postfix = infixToPostfix(explicitRegex);
    
    Node* root = buildTree(postfix);
    
    cout << "\n=== Syntax Tree Structure ===\n";
    printPrettyTree(root);
    
    cout << "\n=== Node Attributes (Computed from Tree) ===\n";
    printTreeDetails(root);
    
    computeFollowpos(root);
    
    cout << "\n=== Followpos Table ===\n";
    for (int i = 1; i <= leafNodeCount; i++) {
        cout << "Position " << i << " (" << posToSymbol[i] << "):\t";
        printSet(followpos[i]);
        cout << endl;
    }
    
    set<char> symbols;
    for(int i=1; i<=leafNodeCount; i++) {
        if(posToSymbol[i] != '#') symbols.insert(posToSymbol[i]);
    }
    vector<char> alphabet(symbols.begin(), symbols.end());
    
    buildDFA(root, alphabet);
    
    return 0;
}