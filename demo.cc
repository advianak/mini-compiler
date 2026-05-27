#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <string>
#include "lexer.h"
#include "execute.h"

class Builder {
  public:
    InstructionNode* build();

  private:
    LexicalAnalyzer lexer;
    std::unordered_map<std::string, int> symbolTable;

    static InstructionNode* append(InstructionNode* listHead, InstructionNode* listTail) 
    {

        if (!listHead) return listTail;
        InstructionNode* cursor = listHead;

        while (cursor->next) cursor = cursor->next;
        cursor->next = listTail;
        return listHead;
    }

    static InstructionNode* makeNoOp() 
    {

        auto* node = new InstructionNode;
        node->type = NOOP;

        node->next = nullptr;
        return node;
    }

    static InstructionNode* makeJump(InstructionNode* target) 
    {
        auto* node = new InstructionNode;
        node->type = JMP;
        node->jmp_inst.target = target;
        node->next = nullptr;
        return node;
    }


    static InstructionNode* makeCondJump(ConditionalOperatorType relop,int leftIndex,int rightIndex, InstructionNode* destination) 
    {
        auto* node = new InstructionNode;
        node->type = CJMP;
        node->cjmp_inst.condition_op = relop;
        node->cjmp_inst.op1_loc = leftIndex;
        node->cjmp_inst.op2_loc = rightIndex;
        node->cjmp_inst.target = destination;
        node->next = nullptr;
        return node;
    }

    static InstructionNode* makeAssign(int targetLoc, int operand1Loc,int operand2Loc, ArithmeticOperatorType op) 
    {
        auto* node = new InstructionNode;
        node->type = ASSIGN;
        node->assign_inst.lhs_loc = targetLoc;
        node->assign_inst.op1_loc = operand1Loc;
        node->assign_inst.op2_loc = operand2Loc;
        node->assign_inst.op = op;
        node->next = nullptr;
        return node;
    }

    static InstructionNode* makeInput(int varIndex) // make sure to differentiate the input and file output - done
    {
        auto* node = new InstructionNode;
        node->type = IN;
        node->input_inst.var_loc = varIndex;
        node->next = nullptr;
        return node;
    }

   
    static InstructionNode* makeOutput(int varIndex)
    {
        auto* node = new InstructionNode;
        node->type = OUT;
        node->output_inst.var_loc = varIndex;
        node->next = nullptr;
        return node;
    }

    InstructionNode* parseBlock();
    InstructionNode* parseStmtList();
    InstructionNode* parseStatement();
    InstructionNode* parseAssignment();
    InstructionNode* parseSwitch();
    int parseValue();

    ArithmeticOperatorType parseArithmeticOp();
    ConditionalOperatorType parseComparison();
    std::vector<int> collectInputs();
};

// rename the function to understand what youre doing later
InstructionNode* parse_Generate_Intermediate_Representation() 
{
    return Builder().build();
}

InstructionNode* Builder::build() {
    Token token = lexer.GetToken();
    while (token.token_type == ID) {
        int slot = next_available++;
        mem[slot] = 0;

        symbolTable[token.lexeme] = slot;
        token = lexer.peek(1);

        if (token.token_type == COMMA) {
            lexer.GetToken();

            token = lexer.GetToken();
        } else if (token.token_type == SEMICOLON) {
            lexer.GetToken();

            break;
        }
    }

    InstructionNode* program = parseBlock();
    for (int iv : collectInputs()) inputs.push_back(iv);
    program = append(program, makeNoOp());
    return program;
}

InstructionNode* Builder::parseBlock() 
{
    lexer.GetToken();

    InstructionNode* body = parseStmtList();
    lexer.GetToken();
    return body;
}

InstructionNode* Builder::parseStmtList() 
{
    InstructionNode* head = nullptr;

    while (lexer.peek(1).token_type != RBRACE) 
        {
            head = append(head, parseStatement());
        }
    return head;
}

InstructionNode* Builder::parseStatement() 
{
    Token lookahead = lexer.peek(1);
    if (lookahead.token_type == ID)
        return parseAssignment();

    if (lookahead.token_type == INPUT) 
    {
        lexer.GetToken();
        Token id = lexer.GetToken();

        lexer.GetToken();
        return makeInput(symbolTable[id.lexeme]);
    }

    if (lookahead.token_type == OUTPUT) 
    {

        lexer.GetToken();
        Token id = lexer.GetToken();
        lexer.GetToken();
        return makeOutput(symbolTable[id.lexeme]);
    }

    if (lookahead.token_type == IF) 


    {
        lexer.GetToken();
        int left = parseValue();
        auto comp = parseComparison();
        int right = parseValue();

        InstructionNode* after = makeNoOp();
        InstructionNode* cond = makeCondJump(comp, left, right, after);

        InstructionNode* thenBlock = parseBlock();
        thenBlock = append(thenBlock, after);
        cond->next = thenBlock;
        return cond;
    }
    
    if (lookahead.token_type == WHILE) 
    {
        lexer.GetToken();
        int left = parseValue();
        auto comp = parseComparison();
        int right = parseValue();


        InstructionNode* after = makeNoOp();
        InstructionNode* cond = makeCondJump(comp, left, right, after);

        InstructionNode* loopBody = parseBlock();
        loopBody = append(loopBody, makeJump(cond));
        loopBody = append(loopBody, after);
        cond->next = loopBody;
        return cond;
    }

    if (lookahead.token_type == FOR) 
    {
        lexer.GetToken(); lexer.GetToken();
        InstructionNode* init = parseAssignment();

        int left = parseValue();
        auto comp = parseComparison();
        int right = parseValue();
        lexer.GetToken();

        InstructionNode* update = parseAssignment();
        lexer.GetToken();

        InstructionNode* body = parseBlock();
        InstructionNode* after = makeNoOp();
        InstructionNode* cond = makeCondJump(comp, left, right, after);
        InstructionNode* loopSequence = append(init, cond);


        body = append(body, update);
        body = append(body, makeJump(cond));
        body = append(body, after);
        cond->next = body;
        return loopSequence;
    }
    if (lookahead.token_type == SWITCH)
        return parseSwitch();
    return makeNoOp();
}

InstructionNode* Builder::parseAssignment() 
{
    Token id = lexer.GetToken();
    lexer.GetToken();
    int lhs = symbolTable[id.lexeme];
    int operand1 = parseValue();

    ArithmeticOperatorType opType = OPERATOR_NONE;
    int operand2 = 0;
    Token nextTok = lexer.peek(1);

    if (nextTok.token_type == PLUS || nextTok.token_type == MINUS ||
        nextTok.token_type == MULT || nextTok.token_type == DIV) 
        {
        opType = parseArithmeticOp();
        operand2 = parseValue();
    }
    lexer.GetToken();
    return makeAssign(lhs, operand1, operand2, opType);
}

int Builder::parseValue() 
{
    Token tk = lexer.GetToken();
    if (tk.token_type == ID)
        return symbolTable[tk.lexeme];
    int constant= std::atoi(tk.lexeme.c_str());
    int slot= next_available++;
    mem[slot]= constant;
    return slot;
}

ArithmeticOperatorType Builder::parseArithmeticOp() 
{
    Token tk = lexer.GetToken();
    switch (tk.token_type) {
        case PLUS: return OPERATOR_PLUS;
        case MINUS: return OPERATOR_MINUS;
        case MULT: return OPERATOR_MULT;
        case DIV: return OPERATOR_DIV;
        default: return OPERATOR_NONE;
    }
}

ConditionalOperatorType Builder::parseComparison() 
{
    Token tk = lexer.GetToken();
    switch (tk.token_type) {

        case GREATER: return CONDITION_GREATER;
        case LESS: return CONDITION_LESS;
        case NOTEQUAL: return CONDITION_NOTEQUAL;
        default: return CONDITION_NOTEQUAL;
    }
}

InstructionNode* Builder::parseSwitch() 
{
    lexer.GetToken();

    Token varToken = lexer.GetToken();
    int switchIndex = symbolTable[varToken.lexeme];  // fix the switch statement
    lexer.GetToken();

    std::vector<InstructionNode*> tests;
    std::vector<InstructionNode*> breaks;
    InstructionNode* defaultBranch = nullptr;

    while (lexer.peek(1).token_type == CASE) 
    {
        lexer.GetToken();
        Token num = lexer.GetToken();
        int caseValIndex = next_available++; // fix overflow - done
        mem[caseValIndex] = std::atoi(num.lexeme.c_str());
        lexer.GetToken();

        InstructionNode* test = makeCondJump(CONDITION_NOTEQUAL, switchIndex, caseValIndex, nullptr);
        InstructionNode* caseBody = parseBlock();
        test->cjmp_inst.target = caseBody;
        InstructionNode* brk = makeJump(nullptr);

        breaks.push_back(brk); // make sure to add the break statement
        InstructionNode* tail = caseBody;
        while (tail->next) tail = tail->next;
        tail->next = brk;

        tests.push_back(test);
    }

    if (lexer.peek(1).token_type == DEFAULT)
    {
        lexer.GetToken(); lexer.GetToken();
        defaultBranch = parseBlock();
    }

    lexer.GetToken();
    InstructionNode* exitLabel = makeNoOp();

   
    for (size_t i = 0; i < tests.size(); ++i) 
    {
        InstructionNode* currentTest = tests[i];
        if (i + 1 < tests.size()) 
        {

            currentTest->next = tests[i + 1];
        } else {

            if (defaultBranch) 
            {
                currentTest->next = defaultBranch;
            } else {
                currentTest->next = exitLabel;
            }
        }
    }

    InstructionNode* head;
    if (tests.empty()) 
    {
        head = nullptr;
    } else {
        head = tests.front();
    }

    if (defaultBranch) 
    {
        if (!head) head = defaultBranch;

        InstructionNode* dTail = defaultBranch;
        while (dTail->next) dTail = dTail->next;

        dTail->next = exitLabel;
    }

    for (auto* brk : breaks) 
    {
        brk->jmp_inst.target = exitLabel;
    }
    if (head) 
    {
        InstructionNode* tail = head;

        while (tail->next && tail->next != exitLabel) tail = tail->next;
        tail->next = exitLabel;
    } else 
    {
        head = exitLabel;
    }

    return head;
}

std::vector<int> Builder::collectInputs() 
{
    std::vector<int> inputList;
    while (lexer.peek(1).token_type == NUM)
    {
        Token numTok = lexer.GetToken();
        inputList.push_back(std::atoi(numTok.lexeme.c_str()));

    }
    return inputList;
}