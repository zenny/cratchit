#include <iostream>
#include <deque>
#include <variant>
#include <vector>
#include <optional>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <random>
#include <cstdlib>
#include <cctype>
#include <map>

namespace parse {
    using In = std::string_view;

    template <typename P>
    using ParseResult = std::optional<std::pair<P,In>>;

    template <typename P>
    class Parse {
    public:
        virtual ParseResult<P> operator()(In const& in) const = 0;
    };

    class ParseWord : public Parse<std::string> {
    public:
        using P = std::string;
        virtual ParseResult<P> operator()(In const& in) const {
            ParseResult<P> result{};
            P p{};
            auto iter=in.begin();
            for (;iter!=in.end();++iter) if (delimeters.find(*iter)==std::string::npos) break; // skip delimeters
            for (;iter!=in.end();++iter) {
                if (delimeters.find(*iter)!=std::string::npos) break;
                p += *iter;
            }
            if (p.size()>0) result = {p,In{iter,in.end()}}; // empty word = unsuccessful
            return result;
        }
    private:
        std::string const delimeters{" ,.;:="};
    };

    template <typename T>
    auto parse(T const& p,In const& in) {
        return p(in);
    }

}

struct Model {
    std::string prompt{};
	bool quit{};
};

using Command = std::string;
struct Quit {};
struct Nop {};

using Msg = std::variant<Nop,Quit,Command>;

using Ux = std::vector<std::string>;

struct Updater {
	Model const& model;
	Model operator()(Command const& command) {
		Model result{model};
		result.prompt += "\nUpdate for command not yet implemented";
		result.prompt += "\n>";
		return result;
	}
	Model operator()(Quit const& quit) {
		Model result{model};
		result.prompt += "\nBy for now :)";
		result.quit = true;
		return result;
	}
	Model operator()(Nop const& nop) {return model;}
};

using EnvironmentValue = std::string;
using Environment = std::map<std::string,EnvironmentValue>;
std::string to_string(EnvironmentValue const& ev) {
	// Expand to variants when EnvironmentValue is no longer a simple string (if ever?)
	return ev;
}
EnvironmentValue to_value(std::string const s) {
	// Expand to variants when EnvironmentValue is no longer a simple string (if ever?)
	return s;
}
std::string to_string(Environment::value_type const& entry) {
	std::ostringstream os{};
	os << std::quoted(entry.first);
	os << ":";
	os << std::quoted(to_string(entry.second));
	return os.str();
}

namespace tokenize {
	// returns s split into first,second on provided delimiter delim.
	// split fail returns first = "" and second = s
	std::pair<std::string,std::string> split(std::string s,char delim) {
		auto pos = s.find(delim);
		if (pos<s.size()) return {s.substr(0,pos),s.substr(pos+1)};
		else return {"",s}; // split failed (return right = unsplit string)
	}
}

class Cratchit {
public:
	Cratchit(std::filesystem::path const& p) 
		: cratchit_file_path{p}
		  ,environment{environment_from_file(p)} {}
	~Cratchit() {
		environment_to_file(cratchit_file_path);
	}
	Model init() {
		Model result{};
		result.prompt += "\nInit from ";
		result.prompt += cratchit_file_path;
		return result;
	}
	Model update(Msg const& msg,Model const& model) {
		Updater updater{model};
		return std::visit(updater,msg);
	}
	Ux view(Model const& model) {
		Ux result{};
		result.push_back(model.prompt);
		return result;
	}
private:
	std::filesystem::path cratchit_file_path{};
	Environment environment{};
	bool is_value_line(std::string const& line) {
		return (line.size()==0)?false:(line.substr(0,2) != R"(//)");
	}
	Environment environment_from_file(std::filesystem::path const& p) {
		Environment result{};
		try {
			// TEST
			if (true) {
				environment.insert({"Test Entry","Test Value"});
				environment.insert({"Test2","4711"});
			}
			std::ifstream in{p};
			std::string line{};
			while (std::getline(in,line)) {
				if (is_value_line(line)) {
					// Read <name>:<value> where <name> is "bla bla" and <value> is "bla bla " or "StringWithNoSpaces"
					std::istringstream in{line};
					std::string name{},value{};
					char colon{};
					in >> std::quoted(name) >> colon >> std::quoted(value);
					environment.insert({name,to_value(value)});
				}
			}
		}
		catch (std::runtime_error const& e) {
			std::cerr << "\nERROR - Write to " << p << " failed. Exception:" << e.what();
		}
		return result;
	}
	void environment_to_file(std::filesystem::path const& p) {
		try {
			std::ofstream out{p};		
			for (auto iter = environment.begin(); iter != environment.end();++iter) {
				if (iter == environment.begin()) out << to_string(*iter);
				else out << "\n" << to_string(*iter);
			}
		}
		catch (std::runtime_error const& e) {
			std::cerr << "\nERROR - Write to " << p << " failed. Exception:" << e.what();
		}
	}
};

class REPL {
public:
    REPL(std::filesystem::path const& environment_file_path,Command const& command) : cratchit{environment_file_path} {
        this->model = cratchit.init();
        in.push_back(Command{command});
    }
    bool operator++() {
        Msg msg{Nop{}};
        if (in.size()>0) {
            msg = in.back();
            in.pop_back();
        }
        this->model = cratchit.update(msg,this->model);
        auto ux = cratchit.view(this->model);
        for (auto const&  row : ux) std::cout << row;
		if (this->model.quit) return false; // Done
		else {
			Command user_input{};
			std::getline(std::cin,user_input);
			in.push_back(to_msg(user_input));
			return true;
		}
    }
private:
    Msg to_msg(Command const& user_input) {
        if (user_input == "quit" or user_input=="q") return Quit{};
        else return Command{user_input};
    }
    Model model{};
    Cratchit cratchit;
    std::deque<Msg> in{};
};

int main(int argc, char *argv[])
{
    std::string sPATH{std::getenv("PATH")};
    std::cout << "\nPATH=" << sPATH;
    std::string command{};
    for (int i=1;i<argc;i++) command+= std::string{argv[i]} + " ";
    auto current_path = std::filesystem::current_path();
    auto environment_file_path = current_path / "cratchit.env";
    REPL repl{environment_file_path,command};
    while (++repl);
    // std::cout << "\nBye for now :)";
    std::cout << std::endl;
    return 0;
}