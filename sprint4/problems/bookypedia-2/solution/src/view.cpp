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
    output_ << prompt << std::endl;
    
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
    return SelectFromList(authors, "Enter author # or empty line to cancel"s, allow_empty);
}

std::optional<View::BookSelection> View::SelectBook(const std::string& title) {
    if (title.empty()) {
        // Выбор из всех книг
        auto books = use_cases_.GetAllBooks();
        if (books.empty()) {
            return std::nullopt;
        }
        
        auto index = SelectFromList(books, "Enter book # or empty line to cancel"s);
        if (index) {
            BookSelection sel;
            sel.type = BookSelection::GLOBAL_INDEX;
            sel.global_index = *index;
            return sel;
        }
    } else {
        // Показываем книги с указанным названием
        auto details = use_cases_.GetBooksByTitle(title);
        if (details.empty()) {
            output_ << "Book not found: "sv << title << std::endl;
            return std::nullopt;
        }
        
        if (details.size() == 1) {
            // Одна книга
            BookSelection sel;
            sel.type = BookSelection::SINGLE_BOOK;
            sel.title = title;
            sel.author_name = details[0].author_name;
            return sel;
        } else {
            // Несколько книг - выбираем по автору
            std::vector<std::string> book_options;
            for (const auto& detail : details) {
                std::stringstream ss;
                ss << detail.title << " by " << detail.author_name << ", " << detail.year;
                book_options.push_back(ss.str());
            }
            
            auto index = SelectFromList(book_options, 
                "Enter book # or empty line to cancel"s, true);
            if (index) {
                BookSelection sel;
                sel.type = BookSelection::BY_AUTHOR;
                sel.title = title;
                sel.author_index = *index;
                sel.details = details;
                return sel;
            }
        }
    }
    
    return std::nullopt;
}

std::vector<std::string> View::ParseTags(const std::string& tags_str) {
    if (tags_str.empty()) {
        return {};
    }
    
    std::vector<std::string> tags;
    std::vector<std::string> parts;
    boost::split(parts, tags_str, boost::is_any_of(","));
    
    for (auto& tag : parts) {
        boost::algorithm::trim(tag);
        if (!tag.empty()) {
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
            output_ << "Failed to add author: name is empty"sv << std::endl;
            return true;
        }
        
        use_cases_.AddAuthor(name);
        output_ << "Author added successfully"sv << std::endl;
    } catch (const std::exception& e) {
        output_ << "Failed to add author: "sv << e.what() << std::endl;
    }
    return true;
}

bool View::ShowAuthors() {
    try {
        auto authors = use_cases_.GetAllAuthors();
        if (authors.empty()) {
            output_ << "No authors found"sv << std::endl;
        } else {
            for (size_t i = 0; i < authors.size(); ++i) {
                output_ << i + 1 << " " << authors[i] << std::endl;
            }
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show authors: "sv << e.what() << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        
        if (name.empty()) {
            // Выбор по индексу
            auto index = SelectAuthor(true);
            if (index) {
                use_cases_.DeleteAuthorByIndex(*index);
                output_ << "Author deleted successfully"sv << std::endl;
            }
        } else {
            // Удаление по имени
            auto author_id = use_cases_.GetAuthorIdByName(name);
            if (!author_id) {
                output_ << "Author not found: "sv << name << std::endl;
                return true;
            }
            use_cases_.DeleteAuthor(name);
            output_ << "Author deleted successfully"sv << std::endl;
        }
    } catch (const std::exception& e) {
        output_ << "Failed to delete author: "sv << e.what() << std::endl;
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
            // Выбор по индексу
            index = SelectAuthor(true);
            if (!index) {
                return true;
            }
            auto authors = use_cases_.GetAllAuthors();
            author_name = authors[*index];
        } else {
            author_name = old_name;
            // Проверяем, существует ли автор
            auto author_id = use_cases_.GetAuthorIdByName(author_name);
            if (!author_id) {
                output_ << "Author not found: "sv << author_name << std::endl;
                return true;
            }
        }
        
        output_ << "Enter new name: "sv;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);
        
        if (new_name.empty()) {
            output_ << "Failed to edit author: new name is empty"sv << std::endl;
            return true;
        }
        
        if (index) {
            use_cases_.EditAuthorByIndex(*index, new_name);
        } else {
            use_cases_.EditAuthor(author_name, new_name);
        }
        output_ << "Author edited successfully"sv << std::endl;
    } catch (const std::exception& e) {
        output_ << "Failed to edit author: "sv << e.what() << std::endl;
    }
    return true;
}

// Books
bool View::AddBook(std::istream& cmd_input) {
    try {
        std::string line;
        std::getline(cmd_input, line);
        
        // Парсим год и название
        std::istringstream iss(line);
        int year;
        std::string title;
        
        if (!(iss >> year)) {
            output_ << "Failed to add book: invalid year"sv << std::endl;
            return true;
        }
        
        std::getline(iss, title);
        boost::algorithm::trim(title);
        
        if (title.empty()) {
            output_ << "Failed to add book: title is empty"sv << std::endl;
            return true;
        }
        
        // Выбор автора
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
            output_ << "Operation cancelled"sv << std::endl;
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
            output_ << "Failed to find author"sv << std::endl;
            return true;
        }
        
        // Ввод тегов
        output_ << "Enter tags (comma-separated) or empty line: "sv;
        std::string tags_str;
        std::getline(input_, tags_str);
        
        auto tags = ParseTags(tags_str);
        use_cases_.AddBook(*author_id, title, year, tags);
        output_ << "Book added successfully"sv << std::endl;
        
    } catch (const std::exception& e) {
        output_ << "Failed to add book: "sv << e.what() << std::endl;
    }
    return true;
}

bool View::ShowBooks() {
    try {
        auto books = use_cases_.GetAllBooks();
        if (books.empty()) {
            output_ << "No books found"sv << std::endl;
        } else {
            for (const auto& book : books) {
                output_ << book << std::endl;
            }
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show books: "sv << e.what() << std::endl;
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
        auto books = use_cases_.GetBooksByAuthor(authors[*author_index]);
        
        if (books.empty()) {
            output_ << "No books found for this author"sv << std::endl;
        } else {
            for (const auto& book : books) {
                output_ << book << std::endl;
            }
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show author books: "sv << e.what() << std::endl;
    }
    return true;
}

bool View::ShowBook(std::istream& cmd_input) {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        
        auto selection_opt = SelectBook(title);
        if (!selection_opt) {
            return true;
        }
        
        const auto& selection = *selection_opt;
        std::vector<app::BookDetail> details;
        
        switch (selection.type) {
            case BookSelection::SINGLE_BOOK:
                details = use_cases_.GetBooksByTitle(selection.title);
                break;
                
            case BookSelection::BY_AUTHOR:
                if (selection.author_index >= 0 && 
                    selection.author_index < static_cast<int>(selection.details.size())) {
                    details.push_back(selection.details[selection.author_index]);
                }
                break;
                
            case BookSelection::GLOBAL_INDEX: {
                auto all_books = use_cases_.GetAllBooks();
                if (selection.global_index >= 0 && 
                    selection.global_index < static_cast<int>(all_books.size())) {
                    // Извлекаем название из строки
                    std::string line = all_books[selection.global_index];
                    size_t pos = line.find(' ');
                    if (pos != std::string::npos) {
                        size_t by_pos = line.find(" by ", pos);
                        if (by_pos != std::string::npos) {
                            std::string book_title = line.substr(pos + 1, by_pos - pos - 1);
                            details = use_cases_.GetBooksByTitle(book_title);
                        }
                    }
                }
                break;
            }
        }
        
        if (details.empty()) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }
        
        for (const auto& detail : details) {
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
            output_ << std::endl;
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show book: "sv << e.what() << std::endl;
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        
        auto selection_opt = SelectBook(title);
        if (!selection_opt) {
            return true;
        }
        
        const auto& selection = *selection_opt;
        
        switch (selection.type) {
            case BookSelection::SINGLE_BOOK:
                use_cases_.DeleteBook(selection.title, "");
                output_ << "Book deleted successfully"sv << std::endl;
                break;
                
            case BookSelection::BY_AUTHOR:
                if (selection.author_index >= 0 && 
                    selection.author_index < static_cast<int>(selection.details.size())) {
                    use_cases_.DeleteBook(selection.title, 
                                         selection.details[selection.author_index].author_name);
                    output_ << "Book deleted successfully"sv << std::endl;
                }
                break;
                
            case BookSelection::GLOBAL_INDEX:
                use_cases_.DeleteBookByIndex(selection.global_index, -1);
                output_ << "Book deleted successfully"sv << std::endl;
                break;
        }
    } catch (const std::exception& e) {
        output_ << "Failed to delete book: "sv << e.what() << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) {
    try {
        std::string old_title;
        std::getline(cmd_input, old_title);
        boost::algorithm::trim(old_title);
        
        auto selection_opt = SelectBook(old_title);
        if (!selection_opt) {
            return true;
        }
        
        const auto& selection = *selection_opt;
        
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
        
        switch (selection.type) {
            case BookSelection::SINGLE_BOOK:
                use_cases_.EditBook(selection.title, new_title, new_year, new_tags, "");
                output_ << "Book edited successfully"sv << std::endl;
                break;
                
            case BookSelection::BY_AUTHOR:
                if (selection.author_index >= 0 && 
                    selection.author_index < static_cast<int>(selection.details.size())) {
                    use_cases_.EditBook(selection.title, new_title, new_year, new_tags,
                                       selection.details[selection.author_index].author_name);
                    output_ << "Book edited successfully"sv << std::endl;
                }
                break;
                
            case BookSelection::GLOBAL_INDEX:
                use_cases_.EditBookByIndex(selection.global_index, new_title, new_year, new_tags);
                output_ << "Book edited successfully"sv << std::endl;
                break;
        }
    } catch (const std::exception& e) {
        output_ << "Failed to edit book: "sv << e.what() << std::endl;
    }
    return true;
}

}  // namespace ui