#pragma once

class Printer
{
  public:
	virtual void print(const char *fmt, ...) = 0;
	virtual void indent() = 0;
	virtual void unindent() = 0;
	virtual ~Printer()
	{
	}
};


class BasicPrinter : public Printer
{
	size_t m_indent = 0;

  public:
	virtual void print(const char* fmt, ...) override
	{

		char buffer[1024];
		va_list va;
		va_start(va, fmt);
		vsnprintf_s(buffer, sizeof(buffer), fmt, va);
		//fprintf(stdout, "%*s\n", m_indent, buffer);
		for (size_t i = 0; i < m_indent; ++i)
			putchar('\t');
		fprintf(stdout, "%s\n", buffer);
		va_end(va);
	}

	virtual void indent() override
	{
		++m_indent;
	}

	virtual void unindent() override
	{
		if (m_indent > 0)
			--m_indent;
	}
};