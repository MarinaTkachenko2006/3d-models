#ifndef __EXPRESSION_IMD__
#define __EXPRESSION_IMD__

#include <complex>
#include <memory>
#include <set>
#include <string>

namespace Core {

	template <typename T>
	struct VarsT {
		T x{}, y{}, z{};
		T u{}, v{}, t{};
	};

	using RealVars = VarsT<double>;
	using ComplexVars = VarsT<std::complex<double>>;

	template <typename T>
	class ExpressionT {
	public:
		ExpressionT();
		~ExpressionT();
		ExpressionT(ExpressionT&&) noexcept;
		ExpressionT& operator=(ExpressionT&&) noexcept;
		ExpressionT(const ExpressionT&) = delete;
		ExpressionT& operator=(const ExpressionT&) = delete;

		bool Parse(const std::string& text,
			const std::set<std::string>& allowedVars,
			std::string* error = nullptr);
		T Eval(const VarsT<T>& v) const;
		bool Valid() const noexcept { return this->___valid; }
		const std::string& Error() const noexcept { return this->__err; }
		struct Node;

	private:
		std::unique_ptr<Node> __root;
		std::string __text, __err;
		bool ___valid = false;
	};

	using RealExpression = ExpressionT<double>;
	using ComplexExpression = ExpressionT<std::complex<double>>;
}

#endif