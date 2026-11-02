#include "Expression.h"
#include "Constants.h"
#include <cctype>
#include <cmath>

namespace Core {

    template <typename T>
    struct ExpressionT<T>::Node {
        enum Type { Num, Var, Bin, Un, Func };
        Type role = Type::Num;
        T num{};
        std::string name;
        char op = 0;
        std::unique_ptr<Node> a, b, child;

        T Eval(const VarsT<T>& v) const {
            if constexpr (std::is_same_v<T, double>) {
                switch (this->role) {
                case Num: return this->num;
                case Var:
                    if (this->name == "x") return v.x;
                    if (this->name == "y") return v.y;
                    if (this->name == "z") return v.z;
                    if (this->name == "u") return v.u;
                    if (this->name == "v") return v.v;
                    if (this->name == "PI") return PI;
                    if (this->name == "E") return E;
                    if (this->name == "TAU") return TAU;
                    if (this->name == "PHI") return PHI;
                    return 0.0;
                case Un: { double tmp(this->child->Eval(v)); return this->op == '-' ? -tmp : tmp; }
                case Bin: {
                    double left(this->a->Eval(v)), right(this->b->Eval(v));
                    switch (this->op) {
                    case '+': return left + right;
                    case '-': return left - right;
                    case '*': return left * right;
                    case '/': return std::fabs(right) < EPSILON ? 0.0 : left / right;
                    case '^': return std::pow(left, right);
                    }
                    return 0.0;
                }
                case Func: {
                    double x(this->child->Eval(v));
                    if (this->name == "sin") return std::sin(x);
                    if (this->name == "cos") return std::cos(x);
                    if (this->name == "tg") return std::tan(x);
                    if (this->name == "arcsin") return std::asin(x);
                    if (this->name == "arccos") return std::acos(x);
                    if (this->name == "arctg") return std::atan(x);
                    if (this->name == "sh") return std::sinh(x);
                    if (this->name == "ch") return std::cosh(x);
                    if (this->name == "th") return std::tanh(x);
                    if (this->name == "exp") return std::exp(x);
                    if (this->name == "ln") return x > EPSILON ? std::log(x) : 0.0;
                    if (this->name == "log2") return x > EPSILON ? std::log2(x) : 0.0;
                    if (this->name == "lg") return x > EPSILON ? std::log10(x) : 0.0;
                    if (this->name == "sqrt") return x >= EPSILON ? std::sqrt(x) : 0.0;
                    if (this->name == "abs") return std::fabs(x);
                    if (this->name == "floor") return std::floor(x);
                    if (this->name == "ceil") return std::ceil(x);
                    if (this->name == "round") return std::round(x);
                    if (this->name == "sign") return (x > 0) - (x < 0);
                    return 0.0;
                }
                }
                return 0.0;
            }
            return T{};
        }
    };

    template <typename T>
    struct Parser {
        const std::string& src;
        size_t pos = 0;
        std::string err;
        using NodeT = typename ExpressionT<T>::Node;
        Parser(const std::string& s) : src(s) {}
        void Skip() { while (pos < src.size() && std::isspace((unsigned char)src[pos])) ++pos; }
        bool Eof() const { return pos >= src.size(); }
        char Peek() { Skip(); return Eof() ? 0 : src[pos]; }
        char Get() { Skip(); return Eof() ? 0 : src[pos++]; }

        std::unique_ptr<NodeT> Expr() { return AddSub(); }

        std::unique_ptr<NodeT> AddSub() {
            auto left = MulDiv();
            while (true) {
                char c = Peek();
                if (c != '+' && c != '-') break;
                Get();
                auto right = MulDiv();
                auto n = std::make_unique<NodeT>();
                n->role = NodeT::Bin; n->op = c;
                n->a = std::move(left); n->b = std::move(right);
                left = std::move(n);
            }
            return left;
        }
        std::unique_ptr<NodeT> MulDiv() {
            auto left = Pow();
            while (true) {
                char c = Peek();
                if (c != '*' && c != '/') break;
                Get();
                auto right = Pow();
                auto n = std::make_unique<NodeT>();
                n->role = NodeT::Bin; n->op = c;
                n->a = std::move(left); n->b = std::move(right);
                left = std::move(n);
            }
            return left;
        }
        std::unique_ptr<NodeT> Pow() {
            auto left = Unary();
            if (Peek() == '^') {
                Get();
                auto right = Pow();
                auto n = std::make_unique<NodeT>();
                n->role = NodeT::Bin; n->op = '^';
                n->a = std::move(left); n->b = std::move(right);
                return n;
            }
            return left;
        }
        std::unique_ptr<NodeT> Unary() {
            char c = Peek();
            if (c == '+' || c == '-') {
                Get();
                auto child = Unary();
                auto n = std::make_unique<NodeT>();
                n->role = NodeT::Un; n->op = c;
                n->child = std::move(child);
                return n;
            }
            return Primary();
        }
        std::unique_ptr<NodeT> Primary() {
            char c = Peek();
            if (c == '(') {
                Get();
                auto e = Expr();
                if (Peek() != ')') { err = "expected ')'"; return nullptr; }
                Get();
                return e;
            }
            if (std::isdigit((unsigned char)c) || c == '.') {
                size_t start = pos;
                while (!Eof() && (std::isdigit((unsigned char)src[pos]) || src[pos] == '.')) ++pos;
                auto n = std::make_unique<NodeT>();
                n->role = NodeT::Num;
                try {
                    double v = std::stod(src.substr(start, pos - start));
                    n->num = v;
                }
                catch (...) { err = "bad number"; return nullptr; }
                return n;
            }
            if (std::isalpha((unsigned char)c) || c == '_') {
                size_t start = pos;
                while (!Eof() && (std::isalnum((unsigned char)src[pos]) || src[pos] == '_')) ++pos;
                std::string name = src.substr(start, pos - start);
                if (Peek() == '(') {
                    Get();
                    auto arg = Expr();
                    if (Peek() != ')') { err = "expected ')'"; return nullptr; }
                    Get();
                    auto n = std::make_unique<NodeT>();
                    n->role = NodeT::Func;
                    n->name = name;
                    n->child = std::move(arg);
                    return n;
                }
                auto n = std::make_unique<NodeT>();
                n->role = NodeT::Var;
                n->name = name;
                return n;
            }
            err = "unexpected character";
            return nullptr;
        }
    };

    template <typename T>
    bool Validate(const typename ExpressionT<T>::Node* n,
        const std::set<std::string>& allowedVars, std::string& err) {
        if (!n) return true;
        switch (n->role) {
        case ExpressionT<T>::Node::Num: return true;
        case ExpressionT<T>::Node::Var:
            if (n->name == "PI" || n->name == "E" ||
                n->name == "TAU" || n->name == "PHI")
                return true;
            if (allowedVars.count(n->name) == 0) {
                err = "unknown variable: " + n->name;
                return false;
            }
            return true;
        case ExpressionT<T>::Node::Bin:
            return Validate<T>(n->a.get(), allowedVars, err) &&
                Validate<T>(n->b.get(), allowedVars, err);
        case ExpressionT<T>::Node::Un:
            return Validate<T>(n->child.get(), allowedVars, err);
        case ExpressionT<T>::Node::Func:
            if (n->child) return Validate<T>(n->child.get(), allowedVars, err);
            return true;
        }
        return true;
    }

    template <typename T>
    bool ExpressionT<T>::Parse(const std::string& text,
        const std::set<std::string>& allowedVars, std::string* error) {
        this->__text = text;
        this->___valid = false;
        this->__err.clear();
        this->__root.reset();

        if (text.empty()) {
            this->__err = "empty";
            if (error) *error = this->__err;
            return false;
        }

        Parser<T> p(text);
        auto r = p.Expr();
        p.Skip();

        if (r && !p.err.empty()) r.reset();
        if (!r && p.err.empty()) p.err = "parse error";
        if (r && !p.Eof()) { p.err = "unexpected characters"; r.reset(); }
        if (!r) {
            this->__err = p.err;
            if (error) *error = this->__err;
            return false;
        }

        std::string verr;
        if (!Validate<T>(r.get(), allowedVars, verr)) {
            this->__err = verr;
            if (error) *error = this->__err;
            return false;
        }

        this->__root = std::move(r);
        this->___valid = true;
        return true;
    }

    template <typename T>
    T ExpressionT<T>::Eval(const VarsT<T>& v) const {
        return this->__root ? this->__root->Eval(v) : T{};
    }

    template <typename T>
    ExpressionT<T>::ExpressionT() = default;
    template <typename T>
    ExpressionT<T>::~ExpressionT() = default;
    template <typename T>
    ExpressionT<T>::ExpressionT(ExpressionT&&) noexcept = default;
    template <typename T>
    ExpressionT<T>& ExpressionT<T>::operator=(ExpressionT&&) noexcept = default;

    template class ExpressionT<double>;
    template class ExpressionT<std::complex<double>>;
}