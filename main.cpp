#include <cmath>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct Token { std::string text; double number = 0; bool numeric = false; };

std::vector<Token> lex(const std::string& source) {
    std::vector<Token> out;
    for (size_t i = 0; i < source.size();) {
        if (std::isspace(static_cast<unsigned char>(source[i]))) { ++i; continue; }
        if (std::isdigit(static_cast<unsigned char>(source[i])) || source[i] == '.') {
            char* end = nullptr;
            double value = std::strtod(source.c_str() + i, &end);
            if (end == source.c_str() + i || !std::isfinite(value)) throw std::runtime_error("invalid number");
            size_t next = static_cast<size_t>(end - source.c_str());
            out.push_back({source.substr(i, next-i), value, true}); i = next; continue;
        }
        if (std::isalpha(static_cast<unsigned char>(source[i])) || source[i] == '_') {
            size_t start = i++;
            while (i < source.size() && (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) ++i;
            out.push_back({source.substr(start,i-start)}); continue;
        }
        if (std::string("+-*/^(),").find(source[i]) == std::string::npos) throw std::runtime_error("invalid character");
        out.push_back({source.substr(i++,1)});
    }
    out.push_back({"<end>"}); return out;
}

struct Node {
    std::string op;
    double value = 0;
    std::vector<std::unique_ptr<Node>> children;
};
using Ptr = std::unique_ptr<Node>;
Ptr make(const std::string& op, double value = 0) { auto n = std::make_unique<Node>(); n->op=op; n->value=value; return n; }

double evaluateOp(const std::string& op, const std::vector<double>& a) {
    if (op == "+") return a[0]+a[1]; if (op == "-") return a[0]-a[1];
    if (op == "*") return a[0]*a[1]; if (op == "/") { if (a[1] == 0) throw std::runtime_error("division by zero"); return a[0]/a[1]; }
    if (op == "^") return std::pow(a[0],a[1]); if (op == "neg") return -a[0];
    if (op == "sin") return std::sin(a[0]); if (op == "cos") return std::cos(a[0]);
    if (op == "sqrt") return std::sqrt(a[0]); if (op == "abs") return std::abs(a[0]);
    if (op == "min") return std::min(a[0],a[1]); if (op == "max") return std::max(a[0],a[1]);
    throw std::runtime_error("unsupported function: " + op);
}

int arity(const std::string& op) {
    if (op == "neg" || op == "sin" || op == "cos" || op == "sqrt" || op == "abs") return 1;
    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "^" || op == "min" || op == "max") return 2;
    throw std::runtime_error("unsupported function: " + op);
}

Ptr fold(Ptr node) {
    bool constant = true;
    for (auto& child : node->children) { child = fold(std::move(child)); constant &= child->op == "PUSH"; }
    if (constant && !node->children.empty()) {
        std::vector<double> args; for (auto& child : node->children) args.push_back(child->value);
        double value = evaluateOp(node->op,args);
        if (!std::isfinite(value)) throw std::runtime_error("non-finite result");
        return make("PUSH",value);
    }
    return node;
}

class Parser {
    std::vector<Token> tokens; size_t pos=0;
    const Token& peek() const { return tokens[pos]; }
    bool eat(const std::string& text) { if (peek().text == text) { ++pos; return true; } return false; }
    void expect(const std::string& text) { if (!eat(text)) throw std::runtime_error("expected " + text); }
    static int precedence(const std::string& op) { if (op=="+" || op=="-") return 10; if (op=="*" || op=="/") return 20; if (op=="^") return 30; return -1; }
    Ptr primary() {
        if (eat("-")) { auto n=make("neg"); n->children.push_back(expression(30)); return n; }
        if (eat("+")) return expression(30);
        if (eat("(")) { auto n=expression(0); expect(")"); return n; }
        Token token = tokens[pos++];
        if (token.numeric) return make("PUSH",token.number);
        if (!token.text.empty() && (std::isalpha(static_cast<unsigned char>(token.text[0])) || token.text[0]=='_')) {
            if (!eat("(")) return make("LOAD:" + token.text);
            auto n=make(token.text);
            if (!eat(")")) { do { n->children.push_back(expression(0)); } while(eat(",")); expect(")"); }
            if (static_cast<int>(n->children.size()) != arity(token.text)) throw std::runtime_error("wrong argument count");
            return n;
        }
        throw std::runtime_error("expected value");
    }
    Ptr expression(int minimum) {
        auto left=primary();
        while (precedence(peek().text) >= minimum) {
            std::string op=tokens[pos++].text; int power=precedence(op);
            auto n=make(op); n->children.push_back(std::move(left)); n->children.push_back(expression(power+(op=="^"?0:1))); left=std::move(n);
        }
        return left;
    }
public:
    explicit Parser(const std::string& source): tokens(lex(source)) {}
    Ptr parse() { auto node=expression(0); if (peek().text!="<end>") throw std::runtime_error("unexpected token"); return fold(std::move(node)); }
};

struct Instruction { std::string op; double value=0; };
void compile(const Node& n, std::vector<Instruction>& code) {
    for (const auto& child:n.children) compile(*child,code);
    code.push_back({n.op,n.value});
}
double execute(const std::vector<Instruction>& code, const std::map<std::string,double>& vars) {
    std::vector<double> stack;
    for (const auto& ins:code) {
        if (ins.op=="PUSH") { stack.push_back(ins.value); continue; }
        if (ins.op.rfind("LOAD:",0)==0) {
            auto it=vars.find(ins.op.substr(5)); if (it==vars.end()) throw std::runtime_error("missing variable: "+ins.op.substr(5));
            stack.push_back(it->second); continue;
        }
        int count=arity(ins.op); if (stack.size()<static_cast<size_t>(count)) throw std::runtime_error("stack underflow");
        std::vector<double> args(stack.end()-count,stack.end()); stack.resize(stack.size()-count);
        double value=evaluateOp(ins.op,args); if (!std::isfinite(value)) throw std::runtime_error("non-finite result"); stack.push_back(value);
    }
    if (stack.size()!=1) throw std::runtime_error("invalid bytecode"); return stack.back();
}

int main(int argc,char** argv) {
    try {
        if (argc<2) throw std::runtime_error("usage: expr EXPRESSION [name=value ...]");
        std::map<std::string,double> vars;
        for (int i=2;i<argc;++i) {
            std::string assignment=argv[i]; auto eq=assignment.find('=');
            if (eq==std::string::npos || eq==0) throw std::runtime_error("expected name=value");
            std::string value=assignment.substr(eq+1); size_t used=0; double parsed=std::stod(value,&used);
            if (used!=value.size() || !std::isfinite(parsed)) throw std::runtime_error("invalid variable value");
            vars[assignment.substr(0,eq)]=parsed;
        }
        auto ast=Parser(argv[1]).parse(); std::vector<Instruction> code; compile(*ast,code);
        for (const auto& ins:code) std::cout<<ins.op<<(ins.op=="PUSH"?" "+std::to_string(ins.value):"")<<'\n';
        std::cout<<"RESULT "<<std::setprecision(15)<<execute(code,vars)<<'\n'; return 0;
    } catch (const std::exception& ex) { std::cerr<<"Error: "<<ex.what()<<'\n'; return 1; }
}

