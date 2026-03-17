#pragma once
#include <iosfwd>
#include <vector>
#include <string>
#include <optional>

namespace menu {
class Menu;
}

namespace app {
class UseCases;
}

namespace ui {

class View {
public:
    View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output);

private:
    bool AddAuthor(std::istream& cmd_input);
    bool ShowAuthors();
    bool DeleteAuthor(std::istream& cmd_input);
    bool EditAuthor(std::istream& cmd_input);
    
    bool AddBook(std::istream& cmd_input);
    bool ShowBooks();
    bool ShowAuthorBooks();
    bool ShowBook(std::istream& cmd_input);
    bool DeleteBook(std::istream& cmd_input);
    bool EditBook(std::istream& cmd_input);

    std::vector<std::string> ShowAuthorsList();
    std::vector<std::string> ShowBooksList();
    std::optional<size_t> ChooseAuthor(const std::vector<std::string>& authors);
    std::optional<size_t> ChooseBook(const std::vector<std::string>& books);

    menu::Menu& menu_;
    app::UseCases& use_cases_;
    std::istream& input_;
    std::ostream& output_;
};

}  // namespace ui