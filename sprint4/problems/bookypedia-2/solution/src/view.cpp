#include "view.h"

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

#include "menu.h"
#include "use_cases.h"

using namespace std::literals;

namespace ui {

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    
    // Authors
    menu_.AddAction("AddAuthor"s, "<name>"s, "Adds author"s,
        [this](auto& cmd_input) { return AddAuthor(cmd_input); });
    
    menu_.AddAction("ShowAuthors"s, ""s, "Show authors"s,
        [this](std::istream&) { return ShowAuthors(); });
    
    menu_.AddAction("DeleteAuthor"s, "[name]"s, "Deletes author"s,
        [this](auto& cmd_input) { return DeleteAuthor(cmd_input); });
    
    menu_.AddAction("EditAuthor"s, "[old_name]"s, "Edits author"s,
        [this](auto& cmd_input) { return EditAuthor(cmd_input); });
    
    // Books
    menu_.AddAction("AddBook"s, "<year> <title>"s, "Adds book"s,
        [this](auto& cmd_input) { return AddBook(cmd_input); });
    
    menu_.AddAction("ShowBooks"s, ""s, "Show books"s,
        [this](std::istream&) { return ShowBooks(); });
    
    menu_.AddAction("ShowAuthorBooks"s, ""s, "Show books by author"s,
        [this](std::istream&) { return ShowAuthorBooks(); });
    
    menu_.AddAction("ShowBook"s, "[title]"s, "Show book details"s,
        [this](auto& cmd_input) { return ShowBook(cmd_input); });
    
    menu_.AddAction("DeleteBook"s, "[title]"s, "Deletes book"s,
        [this](auto& cmd_input) { return DeleteBook(cmd_input); });
    
    menu_.AddAction("EditBook"s, "[title]"s, "Edits book"s,
        [this](auto& cmd_input) { return EditBook(cmd_input); });
}

// Helper methods
std::optional<int> View::SelectFromList(const std::vector<std::string>& items, 
                                         const std::string& prompt,
                                         bool allow_empty) {
    if (items.empty()) {
        return std::nullopt;
    }
    
    // Выводим список
    for (size_t i = 0; i < items.size(); ++i) {
        output_ << i + 1 << " " << items[i] << std::endl;
    }
    
    output_ << prompt;
    if (allow_empty) {
        output_ << " (empty line to cancel)";
    }
    output_ << ": "sv;
    
    std::string input;
    std::getline(input_, input);
    boost::algorithm::trim(input);
    
    if (input.empty()) {
        if (allow_empty) {
            return std::nullopt;
        }
        output_ << "Invalid choice. Retry attempt."sv << std::endl;
        return SelectFromList(items, prompt, allow_empty);
    }
    
    try {
        int choice = std::stoi(input);
        if (choice >= 1 && choice <= static_cast<int>(items.size())) {
            return choice - 1;
        }
    } catch (...) {}
    
    output_ << "Invalid choice. Retry attempt."sv << std::endl;
    return SelectFromList(items, prompt, allow_empty);
}

std::optional<int> View::SelectAuthor(bool allow_empty) {
    auto authors = use_cases_.GetAllAuthors();
    if (authors.empty()) {
        return std::nullopt;
    }
    return SelectFromList(authors, "Enter author #"s, allow_empty);
}

std::vector<std::string> View::ParseTags(const std::string& tags_str) {
    if (tags_str.empty() || tags_str == "y" || tags_str == "Y" || tags_str == "n" || tags_str == "N") {
        return {};
    }
    
    std::vector<std::string> tags;
    std::vector<std::string> parts;
    boost::split(parts, tags_str, boost::is_any_of(","));
    
    for (auto& tag : parts) {
        boost::algorithm::trim(tag);
        if (!tag.empty() && tag != "y" && tag != "Y" && tag != "n" && tag != "N") {
            tags.push_back(tag);
        }
    }
    
    // Удаляем дубликаты
    std::sort(tags.begin(), tags.end());
    tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
    
    return tags;
}

// Authors
bool View::AddAuthor(std::istream& cmd_input) {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        
        if (name.empty()) {
            output_ << "Failed to add author"sv << std::endl;
            return true;
        }
        
        use_cases_.AddAuthor(name);
    } catch (const std::exception& e) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() {
    try {
        auto authors = use_cases_.GetAllAuthors();
        for (size_t i = 0; i < authors.size(); ++i) {
            output_ << i + 1 << " " << authors[i] << std::endl;
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show authors"sv << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        
        if (name.empty()) {
            auto index = SelectAuthor(true);
            if (index) {
                use_cases_.DeleteAuthorByIndex(*index);
            }
        } else {
            auto author_id = use_cases_.GetAuthorIdByName(name);
            if (!author_id) {
                output_ << "Failed to delete author"sv << std::endl;
                return true;
            }
            use_cases_.DeleteAuthor(name);
        }
    } catch (const std::exception& e) {
        output_ << "Failed to delete author"sv << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) {
    try {
        std::string old_name;
        std::getline(cmd_input, old_name);
        boost::algorithm::trim(old_name);
        
        std::optional<int> index;
        std::string author_name;
        
        if (old_name.empty()) {
            index = SelectAuthor(true);
            if (!index) {
                return true;
            }
            auto authors = use_cases_.GetAllAuthors();
            if (authors.empty() || *index >= static_cast<int>(authors.size())) {
                return true;
            }
            author_name = authors[*index];
        } else {
            author_name = old_name;
            auto author_id = use_cases_.GetAuthorIdByName(author_name);
            if (!author_id) {
                output_ << "Failed to edit author"sv << std::endl;
                return true;
            }
        }
        
        output_ << "Enter new name: "sv;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);
        
        if (new_name.empty()) {
            output_ << "Failed to edit author"sv << std::endl;
            return true;
        }
        
        if (index) {
            use_cases_.EditAuthorByIndex(*index, new_name);
        } else {
            use_cases_.EditAuthor(author_name, new_name);
        }
    } catch (const std::exception& e) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}

// Books
bool View::AddBook(std::istream& cmd_input) {
    try {
        std::string line;
        std::getline(cmd_input, line);
        
        std::istringstream iss(line);
        int year;
        std::string title;
        
        if (!(iss >> year)) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }
        
        std::getline(iss, title);
        boost::algorithm::trim(title);
        
        if (title.empty()) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }
        
        auto authors = use_cases_.GetAllAuthors();
        if (authors.empty()) {
            output_ << "No authors available. Please add an author first."sv << std::endl;
            return true;
        }
        
        output_ << "Select author:"sv << std::endl;
        for (size_t i = 0; i < authors.size(); ++i) {
            output_ << i + 1 << " " << authors[i] << std::endl;
        }
        output_ << "Enter author #: "sv;
        
        std::string choice_str;
        std::getline(input_, choice_str);
        boost::algorithm::trim(choice_str);
        
        if (choice_str.empty()) {
            return true;
        }
        
        int choice = std::stoi(choice_str);
        if (choice < 1 || choice > static_cast<int>(authors.size())) {
            output_ << "Invalid choice"sv << std::endl;
            return true;
        }
        
        std::string author_name = authors[choice - 1];
        auto author_id = use_cases_.GetAuthorIdByName(author_name);
        
        if (!author_id) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }
        
        output_ << "Enter tags (comma-separated) or empty line: "sv;
        std::string tags_str;
        std::getline(input_, tags_str);
        boost::algorithm::trim(tags_str);
        
        auto tags = ParseTags(tags_str);
        use_cases_.AddBook(*author_id, title, year, tags);
        
    } catch (const std::exception& e) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::ShowBooks() {
    try {
        auto books = use_cases_.GetAllBooks();
        for (const auto& book : books) {
            output_ << book << std::endl;
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show books"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthorBooks() {
    try {
        auto author_index = SelectAuthor(true);
        if (!author_index) {
            return true;
        }
        
        auto authors = use_cases_.GetAllAuthors();
        if (authors.empty() || *author_index >= static_cast<int>(authors.size())) {
            return true;
        }
        
        auto books = use_cases_.GetBooksByAuthor(authors[*author_index]);
        
        for (const auto& book : books) {
            output_ << book << std::endl;
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show author books"sv << std::endl;
    }
    return true;
}

bool View::ShowBook(std::istream& cmd_input) {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        
        if (title.empty()) {
            auto books = use_cases_.GetAllBooks();
            if (books.empty()) {
                return true;
            }
            
            auto index = SelectFromList(books, "Enter book #"s, true);
            if (!index) {
                return true;
            }
            
            std::string line = books[*index];
            size_t pos = line.find(' ');
            if (pos != std::string::npos) {
                size_t by_pos = line.find(" by ", pos);
                if (by_pos != std::string::npos) {
                    title = line.substr(pos + 1, by_pos - pos - 1);
                }
            }
        }
        
        auto details = use_cases_.GetBooksByTitle(title);
        if (details.empty()) {
            output_ << "Book not found: "sv << title << std::endl;
            return true;
        }
        
        if (details.size() > 1) {
            std::vector<std::string> options;
            for (const auto& d : details) {
                std::stringstream ss;
                ss << d.title << " by " << d.author_name << ", " << d.year;
                options.push_back(ss.str());
            }
            
            auto index = SelectFromList(options, "Enter book #"s, true);
            if (!index) {
                return true;
            }
            
            const auto& detail = details[*index];
            output_ << "Title: "sv << detail.title << std::endl;
            output_ << "Author: "sv << detail.author_name << std::endl;
            output_ << "Publication year: "sv << detail.year << std::endl;
            if (!detail.tags.empty()) {
                output_ << "Tags: "sv;
                for (size_t i = 0; i < detail.tags.size(); ++i) {
                    if (i > 0) output_ << ", ";
                    output_ << detail.tags[i];
                }
                output_ << std::endl;
            }
        } else {
            const auto& detail = details[0];
            output_ << "Title: "sv << detail.title << std::endl;
            output_ << "Author: "sv << detail.author_name << std::endl;
            output_ << "Publication year: "sv << detail.year << std::endl;
            if (!detail.tags.empty()) {
                output_ << "Tags: "sv;
                for (size_t i = 0; i < detail.tags.size(); ++i) {
                    if (i > 0) output_ << ", ";
                    output_ << detail.tags[i];
                }
                output_ << std::endl;
            }
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show book"sv << std::endl;
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        
        if (title.empty()) {
            auto books = use_cases_.GetAllBooks();
            if (books.empty()) {
                return true;
            }
            
            auto index = SelectFromList(books, "Enter book #"s, true);
            if (index) {
                use_cases_.DeleteBookByIndex(*index, -1);
            }
            return true;
        }
        
        auto details = use_cases_.GetBooksByTitle(title);
        if (details.empty()) {
            output_ << "Book not found: "sv << title << std::endl;
            return true;
        }
        
        if (details.size() > 1) {
            std::vector<std::string> options;
            for (const auto& d : details) {
                std::stringstream ss;
                ss << d.title << " by " << d.author_name << ", " << d.year;
                options.push_back(ss.str());
            }
            
            auto index = SelectFromList(options, "Enter book #"s, true);
            if (index) {
                use_cases_.DeleteBook(title, details[*index].author_name);
            }
        } else {
            use_cases_.DeleteBook(title, "");
        }
    } catch (const std::exception& e) {
        output_ << "Failed to delete book"sv << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) {
    try {
        std::string old_title;
        std::getline(cmd_input, old_title);
        boost::algorithm::trim(old_title);
        
        if (old_title.empty()) {
            auto books = use_cases_.GetAllBooks();
            if (books.empty()) {
                return true;
            }
            
            auto index = SelectFromList(books, "Enter book #"s, true);
            if (!index) {
                return true;
            }
            
            output_ << "Enter new title (empty to keep current): "sv;
            std::string new_title;
            std::getline(input_, new_title);
            boost::algorithm::trim(new_title);
            
            output_ << "Enter new year (empty to keep current): "sv;
            std::string new_year;
            std::getline(input_, new_year);
            boost::algorithm::trim(new_year);
            
            output_ << "Enter new tags (comma-separated, empty to keep current): "sv;
            std::string tags_str;
            std::getline(input_, tags_str);
            boost::algorithm::trim(tags_str);
            
            auto new_tags = ParseTags(tags_str);
            use_cases_.EditBookByIndex(*index, new_title, new_year, new_tags);
            return true;
        }
        
        auto details = use_cases_.GetBooksByTitle(old_title);
        if (details.empty()) {
            output_ << "Book not found: "sv << old_title << std::endl;
            return true;
        }
        
        output_ << "Enter new title (empty to keep current): "sv;
        std::string new_title;
        std::getline(input_, new_title);
        boost::algorithm::trim(new_title);
        
        output_ << "Enter new year (empty to keep current): "sv;
        std::string new_year;
        std::getline(input_, new_year);
        boost::algorithm::trim(new_year);
        
        output_ << "Enter new tags (comma-separated, empty to keep current): "sv;
        std::string tags_str;
        std::getline(input_, tags_str);
        boost::algorithm::trim(tags_str);
        
        auto new_tags = ParseTags(tags_str);
        
        if (details.size() > 1) {
            std::vector<std::string> options;
            for (const auto& d : details) {
                std::stringstream ss;
                ss << d.title << " by " << d.author_name << ", " << d.year;
                options.push_back(ss.str());
            }
            
            auto index = SelectFromList(options, "Enter book #"s, true);
            if (index) {
                use_cases_.EditBook(old_title, new_title, new_year, new_tags,
                                   details[*index].author_name);
            }
        } else {
            use_cases_.EditBook(old_title, new_title, new_year, new_tags, "");
        }
    } catch (const std::exception& e) {
        output_ << "Failed to edit book"sv << std::endl;
    }
    return true;
}

}  // namespace ui