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
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s,
        [this](auto& cmd_input) { return AddAuthor(cmd_input); });
    
    menu_.AddAction("ShowAuthors"s, ""s, "Show authors"s,
        [this](std::istream& is) { return ShowAuthors(is); });
    
    menu_.AddAction("DeleteAuthor"s, "[name]"s, "Deletes author"s,
        [this](auto& cmd_input) { return DeleteAuthor(cmd_input); });
    
    menu_.AddAction("EditAuthor"s, "[old_name]"s, "Edits author"s,
        [this](auto& cmd_input) { return EditAuthor(cmd_input); });
    
    // Books
    menu_.AddAction("AddBook"s, "<year> <title>"s, "Adds book"s,
        [this](auto& cmd_input) { return AddBook(cmd_input); });
    
    menu_.AddAction("ShowBooks"s, ""s, "Show books"s,
        [this](std::istream& is) { return ShowBooks(is); });
    
    menu_.AddAction("ShowAuthorBooks"s, ""s, "Show books by author"s,
        [this](std::istream& is) { return ShowAuthorBooks(is); });
    
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
    if(items.empty()) {
        return std::nullopt;
    }
    
    // Статическая переменная для отслеживания первой попытки
    static bool first_attempt = true;
    if(first_attempt) {
        for(size_t i = 0; i < items.size(); ++i) {
            output_ << i+1 << " " << items[i] << std::endl;
        }
        output_ << prompt << std::endl;
        first_attempt = false;
    }
    
    std::string input;
    std::getline(input_, input);
    boost::algorithm::trim(input);
    
    if(input.empty()) {
        first_attempt = true;
        if(allow_empty) {
            return std::nullopt;
        }
        output_ << "Invalid choice. Retry attempt."sv << std::endl;
        return SelectFromList(items, prompt, allow_empty);
    }
    
    try {
        int choice = std::stoi(input);
        if(choice >= 1 && choice <= static_cast<int>(items.size())) {
            first_attempt = true;
            return choice - 1;
        }
    } catch(...) {}
    
    output_ << "Invalid choice. Retry attempt."sv << std::endl;
    return SelectFromList(items, prompt, allow_empty);
}

std::optional<int> View::SelectAuthor(bool allow_empty) {
    auto authors = use_cases_.GetAllAuthors();
    if(authors.empty()) {
        return std::nullopt;
    }
    return SelectFromList(authors, "Enter author # or empty line to cancel"s, allow_empty);
}

std::optional<std::pair<int, int>> View::SelectBook(const std::string& title) {
    auto books = use_cases_.GetAllBooks();
    if(books.empty()) {
        return std::nullopt;
    }
    
    if(title.empty()) {
        // Выбор из всех книг
        auto index = SelectFromList(books, "Enter book # or empty line to cancel"s);
        if(index) {
            return std::make_pair(*index, -1);
        }
    } else {
        // Показываем книги с указанным названием
        auto details = use_cases_.GetBooksByTitle(title);
        if(details.empty()) {
            output_ << "Book not found: "sv << title << std::endl;
            return std::nullopt;
        }
        
        if(details.size() == 1) {
            // Одна книга - выбираем её
            auto all_books = use_cases_.GetAllBooks();
            for(size_t i = 0; i < all_books.size(); ++i) {
                if(all_books[i].find(title) != std::string::npos) {
                    return std::make_pair(static_cast<int>(i), -1);
                }
            }
        } else {
            // Несколько книг - выбираем по автору
            std::vector<std::string> book_options;
            for(const auto& detail : details) {
                std::stringstream ss;
                ss << detail.title << " by " << detail.author_name << ", " << detail.year;
                book_options.push_back(ss.str());
            }
            
            auto index = SelectFromList(book_options, 
                "Enter book # or empty line to cancel"s, true);
            if(index) {
                return std::make_pair(-1, *index);
            }
        }
    }
    
    return std::nullopt;
}

std::vector<std::string> View::ParseTags(const std::string& tags_str) {
    if(tags_str.empty()) {
        return {};
    }
    
    std::vector<std::string> tags;
    std::vector<std::string> parts;
    boost::split(parts, tags_str, boost::is_any_of(","));
    
    for(auto& tag : parts) {
        boost::algorithm::trim(tag);
        if(!tag.empty()) {
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
        
        if(name.empty()) {
            output_ << "Failed to add author"sv << std::endl;
            return true;
        }
        
        use_cases_.AddAuthor(name);
    } catch (const std::exception& e) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors(std::istream&) {
    try {
        auto authors = use_cases_.GetAllAuthors();
        for(size_t i = 0; i < authors.size(); ++i) {
            output_ << i+1 << " " << authors[i] << std::endl;
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
        
        if(name.empty()) {
            // Выбор по индексу
            auto index = SelectAuthor(true);
            if(index) {
                use_cases_.DeleteAuthorByIndex(*index);
            }
            // Если index == nullopt (пустой ввод), просто ничего не делаем
        } else {
            // Удаление по имени
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
        
        if(old_name.empty()) {
            // Выбор по индексу
            index = SelectAuthor(true);
            if(!index) {
                return true;
            }
        }
        
        output_ << "Enter new name: "sv;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);
        
        if(new_name.empty()) {
            output_ << "Failed to edit author"sv << std::endl;
            return true;
        }
        
        if(index) {
            use_cases_.EditAuthorByIndex(*index, new_name);
        } else {
            use_cases_.EditAuthor(old_name, new_name);
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
        boost::algorithm::trim(line);
        
        // Парсим год и название
        std::istringstream iss(line);
        int year;
        iss >> year;
        
        std::string title;
        std::getline(iss, title);
        boost::algorithm::trim(title);
        
        if(title.empty()) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }
        
        // Выбор автора
        output_ << "Select author:"sv << std::endl;
        auto author_index = SelectAuthor(false);
        if(!author_index) {
            return true;
        }
        
        auto authors = use_cases_.GetAllAuthors();
        std::string author_name = authors[*author_index];
        auto author_id = use_cases_.GetAuthorIdByName(author_name);
        
        if(!author_id) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }
        
        // Ввод тегов
        output_ << "Enter tags (comma-separated) or empty line: "sv;
        std::string tags_str;
        std::getline(input_, tags_str);
        
        auto tags = ParseTags(tags_str);
        
        use_cases_.AddBook(*author_id, title, year, tags);
    } catch (const std::exception& e) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::ShowBooks(std::istream&) {
    try {
        auto books = use_cases_.GetAllBooks();
        for(const auto& book : books) {
            output_ << book << std::endl;
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show books"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthorBooks(std::istream&) {
    try {
        auto author_index = SelectAuthor(true);
        if(!author_index) {
            return true;
        }
        
        auto authors = use_cases_.GetAllAuthors();
        auto books = use_cases_.GetBooksByAuthor(authors[*author_index]);
        
        for(const auto& book : books) {
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
        
        auto selection = SelectBook(title);
        if(!selection) {
            return true;
        }
        
        auto [book_index, title_index] = *selection;
        
        std::vector<app::BookDetail> details;
        
        if(title_index != -1) {
            // Выбрана конкретная книга из нескольких с одинаковым названием
            auto books_by_title = use_cases_.GetBooksByTitle(title);
            if(title_index >= 0 && title_index < static_cast<int>(books_by_title.size())) {
                details.push_back(books_by_title[title_index]);
            }
        } else if(book_index != -1) {
            // Выбрана книга по глобальному индексу
            auto all_books = use_cases_.GetAllBooks();
            if(book_index >= 0 && book_index < static_cast<int>(all_books.size())) {
                // Извлекаем название из строки
                std::string line = all_books[book_index];
                size_t pos = line.find(' ');
                if(pos != std::string::npos) {
                    size_t by_pos = line.find(" by ", pos);
                    if(by_pos != std::string::npos) {
                        std::string book_title = line.substr(pos + 1, by_pos - pos - 1);
                        details = use_cases_.GetBooksByTitle(book_title);
                    }
                }
            }
        }
        
        if(details.empty()) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }
        
        for(const auto& detail : details) {
            output_ << "Title: "sv << detail.title << std::endl;
            output_ << "Author: "sv << detail.author_name << std::endl;
            output_ << "Publication year: "sv << detail.year << std::endl;
            if(!detail.tags.empty()) {
                output_ << "Tags: "sv;
                for(size_t i = 0; i < detail.tags.size(); ++i) {
                    if(i > 0) output_ << ", ";
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
        
        auto selection = SelectBook(title);
        if(!selection) {
            return true;
        }
        
        auto [book_index, title_index] = *selection;
        
        if(title_index != -1) {
            // Удаляем конкретную книгу из нескольких с одинаковым названием
            auto details = use_cases_.GetBooksByTitle(title);
            if(title_index >= 0 && title_index < static_cast<int>(details.size())) {
                use_cases_.DeleteBook(title, details[title_index].author_name);
            }
        } else if(book_index != -1) {
            // Удаляем книгу по глобальному индексу
            use_cases_.DeleteBookByIndex(book_index, -1);
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
        
        auto selection = SelectBook(old_title);
        if(!selection) {
            return true;
        }
        
        auto [book_index, title_index] = *selection;
        
        // Получаем новые данные
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
        
        auto new_tags = ParseTags(tags_str);
        
        if(title_index != -1) {
            // Редактируем конкретную книгу из нескольких с одинаковым названием
            auto details = use_cases_.GetBooksByTitle(old_title);
            if(title_index >= 0 && title_index < static_cast<int>(details.size())) {
                use_cases_.EditBook(old_title, new_title, new_year, new_tags, 
                                   details[title_index].author_name);
            }
        } else if(book_index != -1) {
            // Редактируем книгу по глобальному индексу
            use_cases_.EditBookByIndex(book_index, new_title, new_year, new_tags);
        }
    } catch (const std::exception& e) {
        output_ << "Failed to edit book"sv << std::endl;
    }
    return true;
}

}  // namespace ui