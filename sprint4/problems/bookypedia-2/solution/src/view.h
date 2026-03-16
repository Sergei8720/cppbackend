#pragma once
#include <iosfwd>
#include <vector>
#include <string>
#include <optional>
#include <functional>

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
    // Authors
    bool AddAuthor(std::istream& cmd_input);
    bool ShowAuthors(std::istream& = {});
    bool DeleteAuthor(std::istream& cmd_input);
    bool EditAuthor(std::istream& cmd_input);
    
    // Books
    bool AddBook(std::istream& cmd_input);
    bool ShowBooks(std::istream& = {});
    bool ShowAuthorBooks(std::istream& = {});
    bool ShowBook(std::istream& cmd_input);
    bool DeleteBook(std::istream& cmd_input);
    bool EditBook(std::istream& cmd_input);

    // Helper methods
    std::optional<int> SelectFromList(const std::vector<std::string>& items, 
                                       const std::string& prompt,
                                       bool allow_empty = true);
    std::optional<int> SelectAuthor(bool allow_empty = true);
    std::optional<std::pair<int, int>> SelectBook(const std::string& title = "");
    std::vector<std::string> ParseTags(const std::string& tags_str);

    menu::Menu& menu_;
    app::UseCases& use_cases_;
    std::istream& input_;
    std::ostream& output_;
};

}  // namespace ui