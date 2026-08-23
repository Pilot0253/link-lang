#include "parser.h"
#include "types.h" 
#include <stdexcept>
#include <iostream>

Parser::Parser(const std::vector<Token>& t) : tokens(t), current(0) {}

const Token& Parser::peek() const { return tokens[current]; }
const Token& Parser::advance() { if (!isAtEnd()) current++; return tokens[current - 1]; }
bool Parser::match(TokenType type) { 
    if (isAtEnd()) return false;
    if (peek().type == type) { advance(); return true; } 
    return false;
}
const Token& Parser::consume(TokenType type, const std::string& err) { 
    if (match(type)) return tokens[current - 1]; 
    throw ChainError(peek().line, peek().column, err + " (Found: '" + peek().value + "')");
}
bool Parser::isAtEnd() const { return peek().type == TokenType::EOF_TOKEN; }

std::unique_ptr<Program> Parser::parse() {
    auto program = std::make_unique<Program>();
    while (!isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) program->statements.push_back(std::move(stmt));
    }
    return program;
}

std::vector<std::unique_ptr<Expr>> Parser::parseArguments() {
    std::vector<std::unique_ptr<Expr>> args;
    if (match(TokenType::LPAREN)) {
        if (peek().type != TokenType::RPAREN) {
            do {
                args.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN, "Expected ')' after arguments");
    }
    return args;
}

std::unique_ptr<Stmt> Parser::parseStatement() {
    while (peek().type == TokenType::NEWLINE || peek().type == TokenType::INDENT || peek().type == TokenType::DEDENT) {
        advance();
        if (isAtEnd()) return nullptr;
    }
    
    if (match(TokenType::CLEAR) || match(TokenType::CLS)) return std::make_unique<ClearStmt>();
    if (match(TokenType::CLASS)) return parseClass(); 
    if (match(TokenType::IMPORT)) {
    std::string path = consume(TokenType::STRING, "Expected file path (string) after import").value;
    return std::make_unique<ImportStmt>(path);
}

    if (match(TokenType::MATCH)) {
        return parseMatch();
    }

    // --- EXTERN C++ PARSING LOGIC ---
    if (match(TokenType::EXTERN)) {
        consume(TokenType::STRING, "Expect language type after extern (e.g., \"c\")");
        std::string lang = previous().value;
        
        // Capture optional flags
        std::string flags = "";
        if (peek().type == TokenType::STRING) {
            flags = advance().value; 
        }
        
        consume(TokenType::LBRACE, "Expect '{' before extern code block");
        
        std::string rawCode = "";
        if (peek().type == TokenType::STRING) {
            rawCode = advance().value;
        }
        
        consume(TokenType::RBRACE, "Expect '}' after extern block");
        
        // Ensure there are 3 arguments here: lang, flags, rawCode
        return std::make_unique<ExternStmt>(lang, flags, rawCode);
    }
    // ---------------------------------

    if (match(TokenType::SET)) 		return parseSet(); 
    if (match(TokenType::APP)) 		return parseApp();
    if (match(TokenType::RETURN)) 	return parseReturn(); 
    if (match(TokenType::FOR)) 		return parseFor();
    if (match(TokenType::WHILE)) 	return parseWhile(); 
    if (match(TokenType::IF)) 		return parseIf(); 
    if (match(TokenType::WINDOW)) 	return parseWindow();
    if (match(TokenType::FUNC)) 	return parseFunc();
    if (match(TokenType::CONNECT)) 	return parseConnect();
    if (match(TokenType::TRY)) 		return parseTry(); 
    if (match(TokenType::BREAK))	return std::make_unique<BreakStmt>();
    if (match(TokenType::CONTINUE))	return std::make_unique<ContinueStmt>(); 
        
    if (match(TokenType::SH)){
        auto command = consume(TokenType::STRING, "Error: 'sh' needs string").value;
        return std::make_unique<PropertyStmt>("sh", command); 
    }

    if (peek().type == TokenType::IDENTIFIER) {
        std::string name = advance().value;
        std::string method = "";
        bool isMethodCall = false;
        if (match(TokenType::DOT)) {
            if (peek().type == TokenType::CLEAR) {
                advance();
                method = "clear";
            } 
            
            else if (peek().type == TokenType::CONNECT) {
				advance();
				method = "connect"; 
			}
            
            else if (peek().type == TokenType::CLS) {
                advance();
                method = "cls";
            }
            else if (peek().type == TokenType::INIT) {
                advance();
                method = "init";
            }
            else {
                method = consume(TokenType::IDENTIFIER, "Expected method").value;
            }

            if (name == "time" || name == "math" || name == "io"   || 
                name == "os"   || name == "str"  || name == "list" ||
                name == "fs"   || name == "term" || name == "dict" ||
                name == "net"  || name == "audio"|| name == "gui"  ||
                name == "webview" || name == "thread" || name == "sync") { 
                
                name += "." + method;
                isMethodCall = false;
            } 
            else {
                isMethodCall = true;
            }
        }

        if (match(TokenType::ASSIGN)) { 
            std::string fullName = name + (isMethodCall ? "." + method : "");
            auto value = parseExpression(); 
            return std::make_unique<SetStmt>(fullName, std::move(value));
        }

        // 2. Handle Increment (i++)
        if (match(TokenType::PLUS_PLUS)) {
            std::string fullName = name + (isMethodCall ? "." + method : "");
            return std::make_unique<UpdateStmt>(fullName, "++"); 
        }

        // 3. Handle Print
        if (name == "print" && !isMethodCall) {
            std::vector<std::unique_ptr<Expr>> args;
            if (peek().type == TokenType::LPAREN) args = parseArguments();
            else args.push_back(parseExpression()); 
            return std::make_unique<CallStmt>("print", std::move(args));
        }

        // 4. Handle Function/Method Call
        if (peek().type == TokenType::LPAREN) {
             auto args = parseArguments();
             if (isMethodCall) {
                 auto objExpr = std::make_unique<VariableExpr>(name);
                 auto methodCall = std::make_unique<MethodCallExpr>(
                     std::move(objExpr),
                     method,
                     std::move(args)
                 );
                 return std::make_unique<ExprStmt>(std::move(methodCall));
             } else {
                 return std::make_unique<CallStmt>(name, std::move(args));
             }
        } else {
             std::vector<std::unique_ptr<Expr>> emptyArgs;
             return std::make_unique<CallStmt>(name, std::move(emptyArgs));
        }
    }
    
    if (!isAtEnd()) {
        throw ChainError(peek().line, peek().column, "Invalid syntax or unexpected token");
    }
    
    return nullptr;
}

std::unique_ptr<Expr> Parser::parseExpression() {
    return parseLogicOr();
}

std::unique_ptr<Expr> Parser::parseLogicOr() {
    auto left = parseLogicAnd();
    while (match(TokenType::OR)) {
        auto right = parseLogicAnd();
        left = std::make_unique<BinaryExpr>('|', std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseLogicAnd() {
    auto left = parseEquality();
    while (match(TokenType::AND)) {
        auto right = parseEquality();
        left = std::make_unique<BinaryExpr>('&', std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseEquality() {
    auto left = parseBitwise();
    while (match(TokenType::LT) || match(TokenType::GT) || 
           match(TokenType::LE) || match(TokenType::GE) || 
           match(TokenType::EQ_EQ) || match(TokenType::BANG_EQ)) {
               
        std::string opStr = tokens[current - 1].value;
        char op = opStr[0];
        if (opStr == "<=") op = '{'; // Map <= to {
        if (opStr == ">=") op = '}'; // Map >= to }
        if (opStr == "==") op = '=';
        if (opStr == "!=") op = '!';
        
        auto right = parseBitwise(); 
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseBitwise() {
    auto left = parseAdditive(); 
    
    while (match(TokenType::BIT_AND) || match(TokenType::BIT_OR) || 
           match(TokenType::XOR) || match(TokenType::LSHIFT) || match(TokenType::RSHIFT)) {
        
        TokenType type = tokens[current - 1].type;
        char opCode = '?';
        if (type == TokenType::BIT_AND) opCode = '&'; 
        if (type == TokenType::BIT_OR)  opCode = '|';
        if (type == TokenType::XOR)     opCode = '^';
        if (type == TokenType::LSHIFT)  opCode = 'L'; 
        if (type == TokenType::RSHIFT)  opCode = 'R'; 
        
        auto right = parseAdditive();
        left = std::make_unique<BinaryExpr>(opCode, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseAdditive() {
    auto left = parseTerm();
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        char op = tokens[current - 1].value[0];
        auto right = parseTerm(); 
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseTerm() {
    auto left = parseUnary();
    while (match(TokenType::STAR) || match(TokenType::SLASH) || match(TokenType::MOD)) {
        char op = tokens[current - 1].value[0];
        auto right = parseUnary();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    if (match(TokenType::MINUS)) {
        auto right = parseUnary();
        return std::make_unique<BinaryExpr>('-', std::make_unique<NumberExpr>(0), std::move(right));
    }
    return parsePostfix();
}

std::unique_ptr<Expr> Parser::parsePostfix() {
    auto expr = parsePrimary();
    while (true) {
        if (match(TokenType::LBRACKET)) {
            auto index = parseExpression();
            consume(TokenType::RBRACKET, "Expected ']' after index");
            expr = std::make_unique<IndexExpr>(std::move(expr), std::move(index));
        } 
        else if (match(TokenType::DOT)) {
            std::string name;

            if (match(TokenType::INIT)) {
                name = "init";
            } else {
                name = consume(TokenType::IDENTIFIER, "Expected property name").value;
            }
            expr = std::make_unique<GetExpr>(std::move(expr), name);
        }
        else if (match(TokenType::LPAREN)) {
            if (auto getExpr = dynamic_cast<GetExpr*>(expr.get())) {
                std::vector<std::unique_ptr<Expr>> args;
                if (peek().type != TokenType::RPAREN) {
                    do { args.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RPAREN, "Expected ')'");
                expr = std::make_unique<MethodCallExpr>(
                    std::move(getExpr->object), 
                    getExpr->name, 
                    std::move(args)
                );
            } else {
                throw std::runtime_error("Invalid call target.");
            }
        }
        else {
            break;
        }
    }
    return expr;
}

#define SKIP_IGNORE \
    while (peek().type == TokenType::NEWLINE || \
           peek().type == TokenType::INDENT || \
           peek().type == TokenType::DEDENT) { advance(); }

std::unique_ptr<Expr> Parser::parsePrimary() {
    if (match(TokenType::THIS)) return std::make_unique<ThisExpr>(tokens[current - 1]);
    
    if (match(TokenType::FUNC)) {
        return std::make_unique<LambdaExpr>(parseFunc());
    }

    if (match(TokenType::NEW)) {
        std::string className = consume(TokenType::IDENTIFIER, "Expected class name").value;
        consume(TokenType::LPAREN, "Expected '(' after class name");
        std::vector<std::unique_ptr<Expr>> args;
        if (peek().type != TokenType::RPAREN) {
            do {
                args.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN, "Expected ')' after arguments");
        return std::make_unique<NewExpr>(className, std::move(args));
    }
	
    if (match(TokenType::TOKEN_NUM)) return std::make_unique<NumberExpr>(std::stoi(tokens[current - 1].value));
    if (match(TokenType::TOKEN_FLOAT)) return std::make_unique<FloatExpr>(std::stod(tokens[current - 1].value));
    if (match(TokenType::STRING)) return std::make_unique<StringExpr>(tokens[current - 1].value);
    if (match(TokenType::CHAR)) return std::make_unique<CharExpr>(tokens[current - 1].value[0]);
    if (match(TokenType::TRUE)) return std::make_unique<BoolExpr>(true);
    if (match(TokenType::FALSE)) return std::make_unique<BoolExpr>(false);
    
    if (match(TokenType::LBRACKET)) {
        std::vector<std::unique_ptr<Expr>> elements;
        SKIP_IGNORE 
        if (peek().type != TokenType::RBRACKET) {
            do {
                SKIP_IGNORE 
                elements.push_back(parseExpression());
                SKIP_IGNORE 
            } while (match(TokenType::COMMA));
        }
        SKIP_IGNORE 
        consume(TokenType::RBRACKET, "Expected ']'");
        return std::make_unique<ArrayExpr>(std::move(elements));
    }

    if (match(TokenType::LBRACE)) {
        std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>> pairs;
        SKIP_IGNORE 
        if (peek().type != TokenType::RBRACE) {
            do {
                SKIP_IGNORE 
                auto key = parseExpression();
                consume(TokenType::COLON, "Expected ':'");
                SKIP_IGNORE 
                auto value = parseExpression();
                pairs.push_back({std::move(key), std::move(value)});
                SKIP_IGNORE 
            } while (match(TokenType::COMMA));
        }
        SKIP_IGNORE 
        consume(TokenType::RBRACE, "Expected '}'");
        return std::make_unique<DictExpr>(std::move(pairs));
    }

    if (peek().type == TokenType::IDENTIFIER) {
        
        std::string name = advance().value;
        if (match(TokenType::DOT)) {
            std::string method = "";
            if (peek().type == TokenType::CONNECT) {
                advance();
                method = "connect"; 
            }
            else if (peek().type == TokenType::CLEAR) {
                advance();
                method = "clear";
            }
            else if (peek().type == TokenType::CLS) {
                advance();
                method = "cls";
            }
            else {
                method = consume(TokenType::IDENTIFIER, "Expected method").value;
            }

            if (name == "time" || name == "math" || name == "io"   || 
                name == "os"   || name == "str"  || name == "list" ||
                name == "fs"   || name == "term" || name == "dict" ||
                name == "net"  || name == "audio"|| name == "gui"  ||
                name == "webview" || name == "thread" || name == "sync") {
                
                name += "." + method; 
                if (peek().type == TokenType::LPAREN) {
                    auto args = parseArguments();
                    return std::make_unique<CallExpr>(name, std::move(args));
                }
                return std::make_unique<VariableExpr>(name);
            } 
            else {
                auto objExpr = std::make_unique<VariableExpr>(name);
                return std::make_unique<GetExpr>(std::move(objExpr), method);
            }
        }
        
        if (peek().type == TokenType::LPAREN) {
            auto args = parseArguments();
            return std::make_unique<CallExpr>(name, std::move(args));
        }
        return std::make_unique<VariableExpr>(name);
    }
    
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpression();
        consume(TokenType::RPAREN, "Butuh ')'");
        return expr;
    }
    throw std::runtime_error("Unknown token: " + peek().value);
}

std::unique_ptr<Stmt> Parser::parseSet() {
    auto target = parsePostfix(); 
    TokenType opType = peek().type; 
    if (opType != TokenType::ASSIGN && 
        opType != TokenType::PLUS_EQ && opType != TokenType::MINUS_EQ &&
        opType != TokenType::STAR_EQ && opType != TokenType::SLASH_EQ &&
        opType != TokenType::MOD_EQ) { 
        throw std::runtime_error("Expected assignment operator ('=', '+=', '-=', etc)");
    }
    
    advance(); 
    auto value = parseExpression(); 
    if (opType != TokenType::ASSIGN) {
        char mathOp = '+';
        if (opType == TokenType::MINUS_EQ) mathOp = '-';
        if (opType == TokenType::STAR_EQ)  mathOp = '*';
        if (opType == TokenType::SLASH_EQ) mathOp = '/';
        if (opType == TokenType::MOD_EQ)   mathOp = '%';
        
        std::unique_ptr<Expr> leftSide = nullptr;
        if (auto v = dynamic_cast<VariableExpr*>(target.get())) {
            leftSide = std::make_unique<VariableExpr>(v->name);
        } 
        else {
             throw std::runtime_error("Compound assignment currently supports simple variables only.");
        }
        value = std::make_unique<BinaryExpr>(mathOp, std::move(leftSide), std::move(value));
    }
    if (auto varExpr = dynamic_cast<VariableExpr*>(target.get())) {
        return std::make_unique<SetStmt>(varExpr->name, std::move(value));
    } 
    else if (auto getExpr = dynamic_cast<GetExpr*>(target.get())) {
        auto setExpr = std::make_unique<SetExpr>(
            std::move(getExpr->object),
            getExpr->name,
            std::move(value)
        );
        return std::make_unique<ExprStmt>(std::move(setExpr));
    }
    else if (auto idxExpr = dynamic_cast<IndexExpr*>(target.get())) {
        return std::make_unique<SetIndexStmt>(
            std::move(idxExpr->object), 
            std::move(idxExpr->index),  
            std::move(value)            
        );
    }

    throw std::runtime_error("Invalid assignment target.");
}


std::unique_ptr<Stmt> Parser::parseWhile() {
    auto condition = parseExpression();
    auto stmt = std::make_unique<WhileStmt>(std::move(condition));

    if (match(TokenType::LBRACE)) {
        while (peek().type != TokenType::RBRACE && !isAtEnd()) {
            if (peek().type == TokenType::NEWLINE || peek().type == TokenType::INDENT || peek().type == TokenType::DEDENT) {
                advance(); continue;
            }
            auto s = parseStatement();
            if (s) stmt->body.push_back(std::move(s));
        }
        consume(TokenType::RBRACE, "Expected '}' after while block");
    } else {
        consume(TokenType::NEWLINE, "Expected newline after while condition");
        consume(TokenType::INDENT, "Expected indent");
        while (!match(TokenType::DEDENT) && !isAtEnd()) {
            if (peek().type == TokenType::NEWLINE) { advance(); continue; }
            auto s = parseStatement();
            if (s) stmt->body.push_back(std::move(s));
        }
    }
    return stmt;
}

std::unique_ptr<AppDecl> Parser::parseApp() {
    std::string name;
    if (peek().type == TokenType::IDENTIFIER || peek().type == TokenType::STRING) name = advance().value;
    else throw std::runtime_error("Error: App-name must be word");
    consume(TokenType::NEWLINE, "Newline needed");
    consume(TokenType::INDENT, "Indent needed");
    auto app = std::make_unique<AppDecl>(name);
    while (!match(TokenType::DEDENT) && !isAtEnd()) {
        auto stmt = parseStatement(); if (stmt) app->body.push_back(std::move(stmt));
    }
    return app;
}

std::unique_ptr<Stmt> Parser::parseIf() {
    auto condition = parseExpression();
    auto stmt = std::make_unique<IfStmt>(std::move(condition));
    if (match(TokenType::LBRACE)) {
        while (peek().type != TokenType::RBRACE && !isAtEnd()) {
            if (peek().type == TokenType::NEWLINE || peek().type == TokenType::INDENT || peek().type == TokenType::DEDENT) {
                advance(); continue;
            }
            auto s = parseStatement();
            if (s) stmt->thenBranch.push_back(std::move(s));
        }
        consume(TokenType::RBRACE, "Expected '}' after if block");
    } else {
        consume(TokenType::NEWLINE, "Expected newline after if condition");
        consume(TokenType::INDENT, "Expected indent");
        while (!match(TokenType::DEDENT) && !isAtEnd()) {
            if (peek().type == TokenType::NEWLINE) { advance(); continue; }
            auto s = parseStatement();
            if (s) stmt->thenBranch.push_back(std::move(s));
        }
    }
    while (peek().type == TokenType::NEWLINE) advance(); 

    if (match(TokenType::ELIF)) {
        stmt->elseBranch.push_back(parseIf());
    } 
    else if (match(TokenType::ELSE)) {
        if (match(TokenType::LBRACE)) {
            while (peek().type != TokenType::RBRACE && !isAtEnd()) {
                if (peek().type == TokenType::NEWLINE || peek().type == TokenType::INDENT || peek().type == TokenType::DEDENT) {
                    advance(); continue;
                }
                auto s = parseStatement();
                if (s) stmt->elseBranch.push_back(std::move(s));
            }
            consume(TokenType::RBRACE, "Expected '}' after else block");
        } else {
            consume(TokenType::NEWLINE, "Expected newline after else");
            consume(TokenType::INDENT, "Expected indent");
            while (!match(TokenType::DEDENT) && !isAtEnd()) {
                if (peek().type == TokenType::NEWLINE) { advance(); continue; }
                auto s = parseStatement();
                if (s) stmt->elseBranch.push_back(std::move(s));
            }
        }
    }
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseFor() {
    auto iteratorName = consume(TokenType::IDENTIFIER, "Expected iterator variable").value;
    consume(TokenType::IN, "Expected 'in' after iterator");
    auto collection = parseExpression();
    
    auto forStmt = std::make_unique<ForStmt>(iteratorName, std::move(collection));

    if (match(TokenType::LBRACE)) {
        while (peek().type != TokenType::RBRACE && !isAtEnd()) {
            if (peek().type == TokenType::NEWLINE || peek().type == TokenType::INDENT || peek().type == TokenType::DEDENT) {
                advance(); continue;
            }
            auto s = parseStatement();
            if (s) forStmt->body.push_back(std::move(s));
        }
        consume(TokenType::RBRACE, "Expected '}' after for block");
    } else {
        consume(TokenType::NEWLINE, "Expected newline");
        consume(TokenType::INDENT, "Expected indent");
        while (!match(TokenType::DEDENT) && !isAtEnd()) {
            if (peek().type == TokenType::NEWLINE) { advance(); continue; }
            auto s = parseStatement();
            if (s) forStmt->body.push_back(std::move(s));
        }
    }
    return forStmt;
}

std::unique_ptr<WindowDecl> Parser::parseWindow() {
    auto name = consume(TokenType::IDENTIFIER, "Expected window name").value;
    consume(TokenType::NEWLINE, "Expected newline");
    consume(TokenType::INDENT, "Expected indent");
    auto window = std::make_unique<WindowDecl>(name);
    while (!match(TokenType::DEDENT) && !isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) window->body.push_back(std::move(stmt));
    }
    return window;
}

std::unique_ptr<FuncDecl> Parser::parseFunc() {
    std::string name = "";
    if (peek().type != TokenType::LPAREN) { 
        if (match(TokenType::INIT)) name = "init";
        else name = consume(TokenType::IDENTIFIER, "Expected func name").value;
    }
    
    std::vector<std::string> params;
    auto func = std::make_unique<FuncDecl>(name, params); 
    
    if (match(TokenType::LPAREN)) {
        if (peek().type != TokenType::RPAREN) {
            do { 
                std::string paramName = consume(TokenType::IDENTIFIER, "Expected param").value;
                func->params.push_back(paramName);

                if (match(TokenType::COLON)) {
                    func->paramTypes.push_back(consume(TokenType::IDENTIFIER, "Expected type").value);
                } else {
                    func->paramTypes.push_back("Any");
                }
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN, "Expected ')'");
    }

    if (match(TokenType::ARROW)) {
        func->returnType = consume(TokenType::IDENTIFIER, "Expected return type").value;
    }

    if (match(TokenType::LBRACE)) {
        while (peek().type != TokenType::RBRACE && !isAtEnd()) {
            if (peek().type == TokenType::NEWLINE || peek().type == TokenType::INDENT || peek().type == TokenType::DEDENT) { advance(); continue; }
            auto stmt = parseStatement();
            if (stmt) func->body.push_back(std::move(stmt));
        }
        consume(TokenType::RBRACE, "Expected '}'");
    } else {
        consume(TokenType::NEWLINE, "Expected newline");
        consume(TokenType::INDENT, "Expected indent");
        while (!match(TokenType::DEDENT) && !isAtEnd()) {
            auto stmt = parseStatement();
            if (stmt) func->body.push_back(std::move(stmt));
        }
    }
    return func;
}

std::unique_ptr<Stmt> Parser::parseClass() {
    std::string name = consume(TokenType::IDENTIFIER, "Expected class name").value;
    
    std::string superclass = "";
    if (match(TokenType::COLON)) { 
        superclass = consume(TokenType::IDENTIFIER, "Expected superclass name").value;
    }
    
    consume(TokenType::LBRACE, "Expected '{'");
    std::vector<std::unique_ptr<FuncDecl>> methods;
    
    while (peek().type != TokenType::RBRACE && !isAtEnd()) {
        if (peek().type == TokenType::NEWLINE || peek().type == TokenType::INDENT || peek().type == TokenType::DEDENT) {
            advance();
            continue;
        }
        
        if (match(TokenType::FUNC) || peek().type == TokenType::INIT) { 
            methods.push_back(parseFunc()); 
        } else { 
            advance(); 
        }
    }
    consume(TokenType::RBRACE, "Expected '}'");
    return std::make_unique<ClassDecl>(name, superclass, std::move(methods));
}

std::unique_ptr<ConnectStmt> Parser::parseConnect() {
    auto source = consume(TokenType::IDENTIFIER, "Expected source").value;
    consume(TokenType::DOT, "Expected dot");
    auto event = consume(TokenType::IDENTIFIER, "Expected event").value;
    consume(TokenType::ARROW, "Expected arrow");
    auto target = consume(TokenType::IDENTIFIER, "Expected target").value;
    return std::make_unique<ConnectStmt>(source, event, target);
}

std::unique_ptr<Stmt> Parser::parseReturn() {
    std::unique_ptr<Expr> value = nullptr;
    if (peek().type != TokenType::NEWLINE && peek().type != TokenType::DEDENT && peek().type != TokenType::EOF_TOKEN) { 
         value = parseExpression();
    }
    if (!isAtEnd() && peek().type == TokenType::NEWLINE) advance();
    return std::make_unique<ReturnStmt>(std::move(value));
}

std::unique_ptr<Stmt> Parser::parseTry() {
    consume(TokenType::LBRACE, "Expected '{' after try");
    std::vector<std::unique_ptr<Stmt>> tryBody;
    
    while (peek().type != TokenType::RBRACE && !isAtEnd()) {
        if (peek().type == TokenType::NEWLINE || peek().type == TokenType::INDENT || peek().type == TokenType::DEDENT) {
            advance(); continue;
        }
        auto s = parseStatement();
        if (s) tryBody.push_back(std::move(s));
    }
    consume(TokenType::RBRACE, "Expected '}' after try block");

    while (peek().type == TokenType::NEWLINE || peek().type == TokenType::INDENT || peek().type == TokenType::DEDENT) {
        advance();
    }

    consume(TokenType::CATCH, "Expected 'catch' after try block");
    consume(TokenType::LPAREN, "Expected '(' after catch");
    std::string errorVar = consume(TokenType::IDENTIFIER, "Expected error variable name").value;
    consume(TokenType::RPAREN, "Expected ')' after error variable");
    
    consume(TokenType::LBRACE, "Expected '{' after catch condition");
    std::vector<std::unique_ptr<Stmt>> catchBody;
    
    while (peek().type != TokenType::RBRACE && !isAtEnd()) {
        if (peek().type == TokenType::NEWLINE || peek().type == TokenType::INDENT || peek().type == TokenType::DEDENT) {
            advance(); continue;
        }
        auto s = parseStatement();
        if (s) catchBody.push_back(std::move(s));
    }
    consume(TokenType::RBRACE, "Expected '}' after catch block");

    return std::make_unique<TryStmt>(std::move(tryBody), std::move(catchBody), errorVar);
}

std::unique_ptr<Stmt> Parser::parseMatch() {
    auto value = parseExpression();
    consume(TokenType::LBRACE, "Expected '{' after match value");
    
    std::vector<MatchCase> cases;
    while (peek().type != TokenType::RBRACE && !isAtEnd()) {
        if (peek().type == TokenType::NEWLINE || peek().type == TokenType::INDENT || peek().type == TokenType::DEDENT) {
            advance();
            continue;
        }
        
        MatchCase mCase;
        if (peek().type == TokenType::IDENTIFIER && peek().value == "_") {
            advance(); 
            mCase.pattern = nullptr; 
        } else {
            mCase.pattern = parseExpression();
        }
        
        consume(TokenType::FAT_ARROW, "Expected '=>' in match case");

        if (match(TokenType::LBRACE)) {
            while (peek().type != TokenType::RBRACE && !isAtEnd()) {
                if (peek().type == TokenType::NEWLINE || peek().type == TokenType::INDENT || peek().type == TokenType::DEDENT) {
                    advance();
                    continue;
                }
                mCase.body.push_back(parseStatement());
            }
            consume(TokenType::RBRACE, "Expected '}' after match case block");
        } else {
            mCase.body.push_back(parseStatement());
        }
        cases.push_back(std::move(mCase));
    }
    consume(TokenType::RBRACE, "Expected '}' after match block");
    return std::make_unique<MatchStmt>(std::move(value), std::move(cases));
}
