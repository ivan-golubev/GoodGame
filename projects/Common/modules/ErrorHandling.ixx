module;
#include <exception>
#include <stdexcept>
#include <string>
#include <windows.h>
export module ErrorHandling;

export namespace gg
{
	class ComException : public std::exception
	{
	public:
		ComException(HRESULT hr);
		std::wstring whatString() const;
		char const* what() const override;
	private:
		HRESULT returnCode;
	};

	void ThrowIfFailed(HRESULT hr);
	void BreakIfFalse(bool);

	class ApplicationInitException : public std::runtime_error
	{
	public:
		ApplicationInitException(std::string const& msg);
	};

	class AssetLoadException : public std::runtime_error
	{
	public:
		AssetLoadException(std::string const& msg);
	};
} // namespace gg
