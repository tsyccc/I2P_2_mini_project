#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
For the language grammar, please refer to Grammar section on the github page:
  https://github.com/lightbulb12294/CSI2P-II-Mini1#grammar
*/

#define MAX_LENGTH 200
typedef enum {
	ASSIGN, ADD, SUB, MUL, DIV, REM, PREINC, PREDEC, POSTINC, POSTDEC, IDENTIFIER, CONSTANT, LPAR, RPAR, PLUS, MINUS, END
} Kind;
typedef enum {
	STMT, EXPR, ASSIGN_EXPR, ADD_EXPR, MUL_EXPR, UNARY_EXPR, POSTFIX_EXPR, PRI_EXPR
} GrammarState;
typedef struct TokenUnit {
	Kind kind;
	int val; // record the integer value or variable name
	struct TokenUnit *next;
} Token;
typedef struct ASTUnit {
	Kind kind;
	int val; // record the integer value or variable name
	struct ASTUnit *lhs, *mid, *rhs;
} AST;

/// utility interfaces

// err marco should be used when a expression error occurs.
#define err(x) {\
	puts("Compile Error!");\
	if(DEBUG) {\
		fprintf(stderr, "Error at line: %d\n", __LINE__);\
		fprintf(stderr, "Error message: %s\n", x);\
	}\
	exit(0);\
}
// You may set DEBUG=1 to debug. Remember setting back to 0 before submit.
#define DEBUG 0
// Split the input char array into token linked list.
Token *lexer(const char *in);
// Create a new token.
Token *new_token(Kind kind, int val);
// Translate a token linked list into array, return its length.
size_t token_list_to_arr(Token **head);
// Parse the token array. Return the constructed AST.
AST *parser(Token *arr, size_t len);
// Parse the token array. Return the constructed AST.
AST *parse(Token *arr, int l, int r, GrammarState S);
// Create a new AST node.
AST *new_AST(Kind kind, int val);
// Find the location of next token that fits the condition(cond). Return -1 if not found. Search direction from start to end.
int findNextSection(Token *arr, int start, int end, int (*cond)(Kind));
// Return 1 if kind is ASSIGN.
int condASSIGN(Kind kind);
// Return 1 if kind is ADD or SUB.
int condADD(Kind kind);
// Return 1 if kind is MUL, DIV, or REM.
int condMUL(Kind kind);
// Return 1 if kind is RPAR.
int condRPAR(Kind kind);
// Check if the AST is semantically right. This function will call err() automatically if check failed.
void semantic_check(AST *now);
// Generate ASM code.
void codegen(AST *root);
// Free the whole AST.
void freeAST(AST *now);

/// debug interfaces

// Print token array.
void token_print(Token *in, size_t len);
// Print AST tree.
void AST_print(AST *head);

char input[MAX_LENGTH];
int reg = 0;
int loaded[3] = {0};
int id_reg[3] = {0};

int main() {
	while (fgets(input, MAX_LENGTH, stdin) != NULL) {
		Token *content = lexer(input);
		size_t len = token_list_to_arr(&content);
		if (len == 0) continue;
		AST *ast_root = parser(content, len);
		semantic_check(ast_root);
		//AST_print(ast_root);
		codegen(ast_root);
		free(content);
		freeAST(ast_root);
		reg = 0;
		for(int i=0 ; i<3 ; i++){
			loaded[i] = 0;
			id_reg[i] = 0;
		}
	}
	return 0;
}

Token *lexer(const char *in) {
	Token *head = NULL;
	Token **now = &head;
	for (int i = 0; in[i]; i++) {
		if (isspace(in[i])) // ignore space characters
			continue;
		else if (isdigit(in[i])) {
			(*now) = new_token(CONSTANT, atoi(in + i));
			while (in[i+1] && isdigit(in[i+1])) i++;
		}
		else if ('x' <= in[i] && in[i] <= 'z') // variable
			(*now) = new_token(IDENTIFIER, in[i]);
		else switch (in[i]) {
			case '=':
				(*now) = new_token(ASSIGN, 0);
				break;
			case '+':
				if (in[i+1] && in[i+1] == '+') {
					i++;
					// In lexer scope, all "++" will be labeled as PREINC.
					(*now) = new_token(PREINC, 0);
				}
				// In lexer scope, all single "+" will be labeled as PLUS.
				else (*now) = new_token(PLUS, 0);
				break;
			case '-':
				if (in[i+1] && in[i+1] == '-') {
					i++;
					// In lexer scope, all "--" will be labeled as PREDEC.
					(*now) = new_token(PREDEC, 0);
				}
				// In lexer scope, all single "-" will be labeled as MINUS.
				else (*now) = new_token(MINUS, 0);
				break;
			case '*':
				(*now) = new_token(MUL, 0);
				break;
			case '/':
				(*now) = new_token(DIV, 0);
				break;
			case '%':
				(*now) = new_token(REM, 0);
				break;
			case '(':
				(*now) = new_token(LPAR, 0);
				break;
			case ')':
				(*now) = new_token(RPAR, 0);
				break;
			case ';':
				(*now) = new_token(END, 0);
				break;
			default:
				err("Unexpected character.");
		}
		now = &((*now)->next);
	}
	return head;
}

Token *new_token(Kind kind, int val) {
	Token *res = (Token*)malloc(sizeof(Token));
	res->kind = kind;
	res->val = val;
	res->next = NULL;
	return res;
}

size_t token_list_to_arr(Token **head) {
	size_t res;
	Token *now = (*head), *del;
	for (res = 0; now != NULL; res++) now = now->next;
	now = (*head);
	if (res != 0) (*head) = (Token*)malloc(sizeof(Token) * res);
	for (int i = 0; i < res; i++) {
		(*head)[i] = (*now);
		del = now;
		now = now->next;
		free(del);
	}
	return res;
}

AST *parser(Token *arr, size_t len) {
	for (int i = 1; i < len; i++) {
		// correctly identify "ADD" and "SUB"
		if (arr[i].kind == PLUS || arr[i].kind == MINUS) {
			switch (arr[i - 1].kind) {
				case PREINC:
				case PREDEC:
				case IDENTIFIER:
				case CONSTANT:
				case RPAR:
					arr[i].kind = arr[i].kind - PLUS + ADD;
				default: break;
			}
		}
	}
	return parse(arr, 0, len - 1, STMT);
}

AST *parse(Token *arr, int l, int r, GrammarState S) {
	AST *now = NULL;
	if (l > r)
		err("Unexpected parsing range.");
	int nxt;
	switch (S) {
		case STMT:
			if (l == r && arr[l].kind == END)
				return NULL;
			else if (arr[r].kind == END)
				return parse(arr, l, r - 1, EXPR);
			else err("Expected \';\' at the end of line.");
		case EXPR:
			return parse(arr, l, r, ASSIGN_EXPR);
		case ASSIGN_EXPR:
			if ((nxt = findNextSection(arr, l, r, condASSIGN)) != -1) {
				now = new_AST(arr[nxt].kind, 0);
				now->lhs = parse(arr, l, nxt - 1, UNARY_EXPR);
				now->rhs = parse(arr, nxt + 1, r, ASSIGN_EXPR);
				return now;
			}
			return parse(arr, l, r, ADD_EXPR);
		case ADD_EXPR:
			if((nxt = findNextSection(arr, r, l, condADD)) != -1) {
				now = new_AST(arr[nxt].kind, 0);
				now->lhs = parse(arr, l, nxt - 1, ADD_EXPR);
				now->rhs = parse(arr, nxt + 1, r, MUL_EXPR);
				return now;
			}
			return parse(arr, l, r, MUL_EXPR);
		case MUL_EXPR:
			// TODO: Implement MUL_EXPR.
			// hint: Take ADD_EXPR as reference.
			if((nxt = findNextSection(arr, r, l, condMUL)) != -1) {
				now = new_AST(arr[nxt].kind, 0);
				now->lhs = parse(arr, l, nxt - 1, MUL_EXPR);
				now->rhs = parse(arr, nxt + 1, r, UNARY_EXPR);
				return now;
			}
			return parse(arr, l, r, UNARY_EXPR);
		case UNARY_EXPR:
			// TODO: Implement UNARY_EXPR.
			// hint: Take POSTFIX_EXPR as reference.
			if (arr[l].kind == PREINC || arr[l].kind == PREDEC || arr[l].kind == PLUS || arr[l].kind == MINUS) {
				now = new_AST(arr[l].kind, 0);
				now->mid = parse(arr, l + 1, r, UNARY_EXPR);
				return now;
			}
			return parse(arr, l, r, POSTFIX_EXPR);
		case POSTFIX_EXPR:
			if (arr[r].kind == PREINC || arr[r].kind == PREDEC) {
				// translate "PREINC", "PREDEC" into "POSTINC", "POSTDEC"
				now = new_AST(arr[r].kind - PREINC + POSTINC, 0);
				now->mid = parse(arr, l, r - 1, POSTFIX_EXPR);
				return now;
			}
			return parse(arr, l, r, PRI_EXPR);
		case PRI_EXPR:
			if (findNextSection(arr, l, r, condRPAR) == r) {
				now = new_AST(LPAR, 0);
				now->mid = parse(arr, l + 1, r - 1, EXPR);
				return now;
			}
			if (l == r) {
				if (arr[l].kind == IDENTIFIER || arr[l].kind == CONSTANT)
					return new_AST(arr[l].kind, arr[l].val);
				err("Unexpected token during parsing.");
			}
			err("No token left for parsing.");
		default:
			err("Unexpected grammar state.");
	}
}

AST *new_AST(Kind kind, int val) {
	AST *res = (AST*)malloc(sizeof(AST));
	res->kind = kind;
	res->val = val;
	res->lhs = res->mid = res->rhs = NULL;
	return res;
}

int findNextSection(Token *arr, int start, int end, int (*cond)(Kind)) {
	int par = 0;
	int d = (start < end) ? 1 : -1;
	for (int i = start; (start < end) ? (i <= end) : (i >= end); i += d) {
		if (arr[i].kind == LPAR) par++;
		if (arr[i].kind == RPAR) par--;
		if (par == 0 && cond(arr[i].kind) == 1) return i;
	}
	return -1;
}

int condASSIGN(Kind kind) {
	return kind == ASSIGN;
}

int condADD(Kind kind) {
	return kind == ADD || kind == SUB;
}

int condMUL(Kind kind) {
	return kind == MUL || kind == DIV || kind == REM;
}

int condRPAR(Kind kind) {
	return kind == RPAR;
}

void semantic_check(AST *now) {
	if (now == NULL) return;
	// Left operand of '=' must be an identifier or identifier with one or more parentheses.
	if (now->kind == ASSIGN) {
		AST *tmp = now->lhs;
		while (tmp->kind == LPAR) tmp = tmp->mid;
		if (tmp->kind != IDENTIFIER)
			err("Lvalue is required as left operand of assignment.");
	}
	// Operand of INC/DEC must be an identifier or identifier with one or more parentheses.
	// TODO: Implement the remaining semantic_check code.
	// hint: Follow the instruction above and ASSIGN-part code to implement.
	// hint: Semantic of each node needs to be checked recursively (from the current node to lhs/mid/rhs node).
	if (now->kind == PREINC || now->kind == PREDEC || now->kind == POSTINC || now->kind == POSTDEC) {
		AST *tmp = now->mid;
		while (tmp->kind == LPAR) tmp = tmp->mid;
		if (tmp->kind != IDENTIFIER)
			err("Increment or decrement operator requires an identifier as operand.");
	}
    semantic_check(now->lhs);
    semantic_check(now->mid);
    semantic_check(now->rhs);
}

//codegen -> getval
//ASSIGN, ADD, SUB, MUL, DIV, REM, PREINC, PREDEC, POSTINC, POSTDEC, IDENTIFIER, CONSTANT, LPAR, RPAR, PLUS, MINUS
/*int reg = 0;
int loaded[3] = {0};
int id_reg[3] = {0};*/

typedef struct _value{
	int type;
	int number;
	int regi;
}value;

typedef enum{NUMBER, REGI} value_type;

value newval(int type, int number, int regi){
	value new;
	new.type = type;
	new.number = number;
	new.regi = regi;
	return new;
}
char* get_op(int kind) {
    switch (kind) {
        case ADD: return "add";
        case SUB: return "sub";
        case MUL: return "mul";
        case DIV: return "div";
        case REM: return "rem";
    }
}

value getval(AST* root){
	if(root ==NULL) return newval(NUMBER, 0, 0);

	value l, r;

	switch (root->kind){
		case CONSTANT:{
			return newval(NUMBER, root->val, 0);
		}
		case IDENTIFIER:{
			int idx = root->val - 'x'; //x=0, y=1, x=2
			if(loaded[idx] == 0){
				loaded[idx] = 1;
				printf("load r%d [%d]\n", reg, idx*4);
				id_reg[idx] = reg;
				reg++;
			}
			return newval(REGI, 0, id_reg[idx]);
		}
		case ADD:
		case SUB:
		case MUL:
		case DIV:
		case REM:{
			l = getval(root->lhs);
			r = getval(root->rhs);
			int newr;
			if (l.type == NUMBER && r.type == NUMBER) {
                int result = 0;
                if (root->kind == ADD) result = l.number + r.number;
                else if (root->kind == SUB) result = l.number - r.number;
                else if (root->kind == MUL) result = l.number * r.number;
                else if (root->kind == DIV) result = l.number / r.number;
                else if (root->kind == REM) result = l.number % r.number;
                return newval(NUMBER, result, 0);
            }
			else if(l.type == NUMBER && r.type == REGI){
				newr = reg++;
				if(l.number >= 0){
					printf("add r%d 0 %d\n", newr, l.number);
				}
				else{
					printf("sub r%d 0 %d\n", newr, 0 - l.number);
				}
				printf("%s r%d r%d r%d\n", get_op(root->kind), newr, newr, r.regi);
			}
			else if(l.type == REGI && r.type == NUMBER){
				newr = reg++;
				if(r.number >= 0){
					printf("add r%d 0 %d\n", newr, r.number);
				}
				else{
					printf("sub r%d 0 %d\n", newr, 0 - r.number);
				}
				printf("%s r%d r%d r%d\n", get_op(root->kind), newr, l.regi, newr);
			}
			else if(l.type == REGI && r.type == REGI){
				newr = reg++;
				printf("%s r%d r%d r%d\n", get_op(root->kind), newr, l.regi, r.regi);
			}
			return newval(REGI, 0, newr);
		}

		case ASSIGN:{
			AST *id = root->lhs;
			while(id->kind == LPAR) id = id->mid;
			int idx = id->val - 'x';
			r = getval(root->rhs);
			if(r.type == NUMBER){
				if(loaded[idx] == 1){
					if(r.number >= 0){
						printf("add r%d 0 %d\n", id_reg[idx], r.number);
					}
					else{
						printf("sub r%d 0 %d\n", id_reg[idx], 0 - r.number);
					}
					printf("store [%d] r%d\n", idx*4, id_reg[idx]);
				}
				else{
					int newr = reg++;
					if(r.number >= 0){
						printf("add r%d 0 %d\n", newr, r.number);
					}
					else{
						printf("sub r%d 0 %d\n", newr, 0 - r.number);
					}
					printf("store [%d] r%d\n", idx*4, newr);
				}
			}
			else{
				if(loaded[idx] == 1){
					printf("add r%d 0 r%d\n", id_reg[idx], r.regi);
					printf("store [%d] r%d\n", idx*4, id_reg[idx]);
				}
				else{
					printf("store [%d] r%d\n", idx*4, r.regi);
				}
			}
			return r;
		}
		case PREINC:{
			AST *id = root->mid;
			while(id->kind == LPAR) id = id->mid;
			int idx = id->val - 'x';
			value operand = getval(id);
			printf("add r%d r%d 1\n", operand.regi, operand.regi);
			printf("store [%d] r%d\n", idx*4, id_reg[idx]);
			return operand;
		}
        case PREDEC:{
			AST *id = root->mid;
			while(id->kind == LPAR) id = id->mid;
			int idx = id->val - 'x';
			value operand = getval(id);
			printf("sub r%d r%d 1\n", operand.regi, operand.regi);
			printf("store [%d] r%d\n", idx*4, id_reg[idx]);
			return operand;
		}
        case POSTINC:{
            AST *id = root->mid;
            while (id->kind == LPAR) id = id->mid;
            int idx = id->val - 'x';
            value operand = getval(id);
            int original_reg = reg++;
            printf("add r%d r%d 0\n", original_reg, operand.regi);// Copy original value
            printf("add r%d r%d 1\n", operand.regi, operand.regi); 
			printf("store [%d] r%d\n", idx*4, id_reg[idx]);
            return newval(REGI, 0, original_reg);
        }
        case POSTDEC:{
            AST *id = root->mid;
            while (id->kind == LPAR) id = id->mid;
            int idx = id->val - 'x';
            value operand = getval(id);
            int original_reg = reg++;
            printf("add r%d r%d 0\n", original_reg, operand.regi);  // Copy original value
            printf("sub r%d r%d 1\n", operand.regi, operand.regi);
			printf("store [%d] r%d\n", idx*4, id_reg[idx]);
            return newval(REGI, 0, original_reg); 
        }
		case LPAR:{
			return getval(root->mid);
		}
		case PLUS:{
			return getval(root->mid);
		}
		case MINUS:{
			value val = getval(root->mid);
			if(val.type == NUMBER){
				return newval(NUMBER, -val.number, 0);
			}
			else{
				printf("sub r%d 0 r%d\n", val.regi, val.regi);
				return val;
			}
		}
	}
}

void codegen(AST *root){
	// TODO: Implement your codegen in your own way.
	// You may modify the function parameter or the return type, even the whole structure as you wish.
	if(root == NULL) return;
	getval(root);

}

void freeAST(AST *now) {
	if (now == NULL) return;
	freeAST(now->lhs);
	freeAST(now->mid);
	freeAST(now->rhs);
	free(now);
}

void token_print(Token *in, size_t len) {
	const static char KindName[][20] = {
		"Assign", "Add", "Sub", "Mul", "Div", "Rem", "Inc", "Dec", "Inc", "Dec", "Identifier", "Constant", "LPar", "RPar", "Plus", "Minus", "End"
	};
	const static char KindSymbol[][20] = {
		"'='", "'+'", "'-'", "'*'", "'/'", "'%'", "\"++\"", "\"--\"", "\"++\"", "\"--\"", "", "", "'('", "')'", "'+'", "'-'"
	};
	const static char format_str[] = "<Index = %3d>: %-10s, %-6s = %s\n";
	const static char format_int[] = "<Index = %3d>: %-10s, %-6s = %d\n";
	for(int i = 0; i < len; i++) {
		switch(in[i].kind) {
			case LPAR:
			case RPAR:
			case PREINC:
			case PREDEC:
			case ADD:
			case SUB:
			case MUL:
			case DIV:
			case REM:
			case ASSIGN:
			case PLUS:
			case MINUS:
				fprintf(stderr, format_str, i, KindName[in[i].kind], "symbol", KindSymbol[in[i].kind]);
				break;
			case CONSTANT:
				fprintf(stderr, format_int, i, KindName[in[i].kind], "value", in[i].val);
				break;
			case IDENTIFIER:
				fprintf(stderr, format_str, i, KindName[in[i].kind], "name", (char*)(&(in[i].val)));
				break;
			case END:
				fprintf(stderr, "<Index = %3d>: %-10s\n", i, KindName[in[i].kind]);
				break;
			default:
				fputs("=== unknown token ===", stderr);
		}
	}
}

void AST_print(AST *head) {
	static char indent_str[MAX_LENGTH] = "  ";
	static int indent = 2;
	const static char KindName[][20] = {
		"Assign", "Add", "Sub", "Mul", "Div", "Rem", "PreInc", "PreDec", "PostInc", "PostDec", "Identifier", "Constant", "Parentheses", "Parentheses", "Plus", "Minus"
	};
	const static char format[] = "%s\n";
	const static char format_str[] = "%s, <%s = %s>\n";
	const static char format_val[] = "%s, <%s = %d>\n";
	if (head == NULL) return;
	char *indent_now = indent_str + indent;
	indent_str[indent - 1] = '-';
	fprintf(stderr, "%s", indent_str);
	indent_str[indent - 1] = ' ';
	if (indent_str[indent - 2] == '`')
		indent_str[indent - 2] = ' ';
	switch (head->kind) {
		case ASSIGN:
		case ADD:
		case SUB:
		case MUL:
		case DIV:
		case REM:
		case PREINC:
		case PREDEC:
		case POSTINC:
		case POSTDEC:
		case LPAR:
		case RPAR:
		case PLUS:
		case MINUS:
			fprintf(stderr, format, KindName[head->kind]);
			break;
		case IDENTIFIER:
			fprintf(stderr, format_str, KindName[head->kind], "name", (char*)&(head->val));
			break;
		case CONSTANT:
			fprintf(stderr, format_val, KindName[head->kind], "value", head->val);
			break;
		default:
			fputs("=== unknown AST type ===", stderr);
	}
	indent += 2;
	strcpy(indent_now, "| ");
	AST_print(head->lhs);
	strcpy(indent_now, "` ");
	AST_print(head->mid);
	AST_print(head->rhs);
	indent -= 2;
	(*indent_now) = '\0';
}
