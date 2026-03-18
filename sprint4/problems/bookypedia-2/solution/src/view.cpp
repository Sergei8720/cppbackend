#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <iostream>
#include <string>
#include <sstream>

#include "menu.h"
#include "use_cases.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s,
        [this](auto& cmd_input) { return AddAuthor(cmd_input); });
    
    menu_.AddAction("ShowAuthors"s, ""s, "Show authors"s,
        [this](auto&) { return ShowAuthors(); });
    
    menu_.AddAction("DeleteAuthor"s, "name"s, "Deletes author"s,
        [this](auto& cmd_input) { return DeleteAuthor(cmd_input); });
    
    menu_.AddAction("EditAuthor"s, "old_name new_name"s, "Edits author"s,
        [this](auto& cmd_input) { return EditAuthor(cmd_input); });
    
    menu_.AddAction("AddBook"s, "year title"s, "Adds book"s,
        [this](auto& cmd_input) { return AddBook(cmd_input); });
    
    menu_.AddAction("ShowBooks"s, ""s, "Show books"s,
        [this](auto&) { return ShowBooks(); });
    
    menu_.AddAction("ShowAuthorBooks"s, ""s, "Show books by author"s,
        [this](auto&) { return ShowAuthorBooks(); });
    
    menu_.AddAction("ShowBook"s, "title"s, "Show book details"s,
        [this](auto& cmd_input) { return ShowBook(cmd_input); });
    
    menu_.AddAction("DeleteBook"s, "title"s, "Deletes book"s,
        [this](auto& cmd_input) { return DeleteBook(cmd_input); });
    
    menu_.AddAction("EditBook"s, "title"s, "Edits book"s,
        [this](auto& cmd_input) { return EditBook(cmd_input); });
}

bool View::AddAuthor(std::istream& cmd_input) {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        if(name.empty()) {
            output_ << "Failed to add author"sv << std::endl;
            return true;
        }
        
        if(use_cases_.GetAuthorIdBy(name)) {
            output_ << "Failed to add author"sv << std::endl;
            return true;
        }
        
        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() {
    try {
        size_t count = 1;
        auto list_of_authors = use_cases_.GetAllAuthors();
        for(auto& item : list_of_authors) {
            output_ << count++ << " " << item << std::endl; 
        }
    } catch (const std::exception&) {
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
            auto list_of_authors = ShowAuthorsList();
            if (list_of_authors.empty()) {
                output_ << "Failed to delete author"sv << std::endl;
                return true;
            }
            auto index = ChooseAuthor(list_of_authors);
            if (!index) {
                output_ << "Failed to delete author"sv << std::endl;
                return true;
            }
            name = list_of_authors[*index - 1];
        }
        
        if (!use_cases_.GetAuthorIdBy(name)) {
            output_ << "Failed to delete author"sv << std::endl;
            return true;
        }
        
        use_cases_.DeleteAuthor(name);
    } catch (const std::exception&) {
        output_ << "Failed to delete author"sv << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) {
    try {
        std::string old_name;
        std::getline(cmd_input, old_name);
        boost::algorithm::trim(old_name);
        
        if (old_name.empty()) {
            auto list_of_authors = ShowAuthorsList();
            if (list_of_authors.empty()) {
                output_ << "Failed to edit author"sv << std::endl;
                return true;
            }
            auto index = ChooseAuthor(list_of_authors);
            if (!index) {
                output_ << "Failed to edit author"sv << std::endl;
                return true;
            }
            old_name = list_of_authors[*index - 1];
        }
        
        output_ << "Enter new name:"sv << std::endl;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);
        
        if (new_name.empty()) {
            output_ << "Failed to edit author"sv << std::endl;
            return true;
        }
        
        if (!use_cases_.GetAuthorIdBy(old_name)) {
            output_ << "Failed to edit author"sv << std::endl;
            return true;
        }
        
        use_cases_.EditAuthor(old_name, new_name);
    } catch (const std::exception&) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) {
    try {
        int year = 0;
        std::string title;
        
        cmd_input >> year;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        
        // Сброс флага eof, если он установлен
        if (cmd_input.eof()) {
            cmd_input.clear();
        }

        auto authors = use_cases_.GetAllAuthors();
        
        // Если авторов нет, предлагаем создать нового
        if (authors.empty()) {
            output_ << "Enter author name or empty line to select from list:" << std::endl;
            std::string author_name;
            std::getline(input_, author_name);
            boost::algorithm::trim(author_name);
            
            if (author_name.empty()) {
                // Отмена добавления
                return true;
            }
            
            output_ << "No author found. Do you want to add " << author_name << " (y/n)?" << std::endl;
            std::string answer;
            std::getline(input_, answer);
            boost::algorithm::trim(answer);
            
            if (answer != "y" && answer != "Y") {
                output_ << "Failed to add book"sv << std::endl;
                return true;
            }
            
            use_cases_.AddAuthor(author_name);
            auto author_id = use_cases_.GetAuthorIdBy(author_name);
            if (author_id) {
                use_cases_.AddBook(*author_id, title, year);
            } else {
                output_ << "Failed to add book"sv << std::endl;
                return true;
            }
        } else {
            // Показываем список существующих авторов
            auto list_of_authors = ShowAuthorsList();
            auto index = ChooseAuthor(list_of_authors);
            if (!index) {
                // Отмена выбора
                return true;
            }
            
            std::string author_name = list_of_authors[*index - 1];
            auto author_id = use_cases_.GetAuthorIdBy(author_name);
            if (!author_id) {
                output_ << "Failed to add book"sv << std::endl;
                return true;
            }
            
            use_cases_.AddBook(*author_id, title, year);
        }
        
        // Запрос тегов
        output_ << "Enter tags (comma-separated) or empty line:" << std::endl;
        std::string tags;
        std::getline(input_, tags);
        boost::algorithm::trim(tags);
        
        // TODO: обработка тегов
        
    } catch (const std::exception& e) {
        output_ << "Failed to add book: " << e.what() << std::endl;
    }
    return true;
}

bool View::ShowBooks() {
    try {
        size_t count = 1;
        auto list_of_books = use_cases_.GetAllBooks();
        for(auto& item : list_of_books) {
            output_ << count++ << " " << item << std::endl; 
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show books: " << e.what() << std::endl;
    }
    return true;
}

bool View::ShowAuthorBooks() {
    try {
        auto list_of_authors = ShowAuthorsList();
        if (list_of_authors.empty()) {
            return true;
        }
        auto index = ChooseAuthor(list_of_authors);
        if (!index) {
            return true;
        }
        auto books = use_cases_.GetBooksBy(list_of_authors[*index - 1]);
        size_t count = 1;
        for (const auto& item : books) {
            output_ << count++ << " " << item << std::endl;
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show books: " << e.what() << std::endl;
    }
    return true;
}

bool View::ShowBook(std::istream& cmd_input) {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        
        if (title.empty()) {
            auto list_of_books = ShowBooksList();
            if (list_of_books.empty()) {
                return true;
            }
            auto index = ChooseBook(list_of_books);
            if (!index) {
                return true;
            }
            // Извлекаем название из строки вида "1 Title by Author, Year"
            std::string book_line = list_of_books[*index - 1];
            size_t pos = book_line.find(' ');
            if (pos != std::string::npos) {
                title = book_line.substr(pos + 1);
                pos = title.find(" by ");
                if (pos != std::string::npos) {
                    title = title.substr(0, pos);
                }
            }
        }
        
        auto book_info = use_cases_.ShowBook(title);
        if (book_info.empty()) {
            output_ << "Book not found" << std::endl;
            return true;
        }
        
        for (const auto& line : book_info) {
            output_ << line << std::endl;
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show book: " << e.what() << std::endl;
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        
        if (title.empty()) {
            auto list_of_books = ShowBooksList();
            if (list_of_books.empty()) {
                output_ << "Failed to delete book"sv << std::endl;
                return true;
            }
            auto index = ChooseBook(list_of_books);
            if (!index) {
                output_ << "Failed to delete book"sv << std::endl;
                return true;
            }
            std::string book_line = list_of_books[*index - 1];
            size_t pos = book_line.find(' ');
            if (pos != std::string::npos) {
                title = book_line.substr(pos + 1);
                pos = title.find(" by ");
                if (pos != std::string::npos) {
                    title = title.substr(0, pos);
                }
            }
        }
        
        use_cases_.DeleteBook(title);
    } catch (const std::exception&) {
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
            auto list_of_books = ShowBooksList();
            if (list_of_books.empty()) {
                output_ << "Failed to edit book"sv << std::endl;
                return true;
            }
            auto index = ChooseBook(list_of_books);
            if (!index) {
                output_ << "Failed to edit book"sv << std::endl;
                return true;
            }
            std::string book_line = list_of_books[*index - 1];
            size_t pos = book_line.find(' ');
            if (pos != std::string::npos) {
                old_title = book_line.substr(pos + 1);
                pos = old_title.find(" by ");
                if (pos != std::string::npos) {
                    old_title = old_title.substr(0, pos);
                }
            }
        }
        
        output_ << "Enter new title:" << std::endl;
        std::string new_title;
        std::getline(input_, new_title);
        boost::algorithm::trim(new_title);
        
        output_ << "Enter new publication year:" << std::endl;
        std::string year_str;
        std::getline(input_, year_str);
        boost::algorithm::trim(year_str);
        std::optional<int> new_year;
        if (!year_str.empty()) {
            try {
                new_year = std::stoi(year_str);
            } catch (...) {
                // ignore
            }
        }
        
        output_ << "Enter new tags (comma-separated):" << std::endl;
        std::string new_tags;
        std::getline(input_, new_tags);
        boost::algorithm::trim(new_tags);
        std::optional<std::string> tags_opt;
        if (!new_tags.empty()) {
            tags_opt = new_tags;
        }
        
        use_cases_.EditBook(old_title, new_title, new_year, tags_opt);
    } catch (const std::exception&) {
        output_ << "Failed to edit book"sv << std::endl;
    }
    return true;
}

std::vector<std::string> View::ShowAuthorsList() {
    auto list_of_authors = use_cases_.GetAllAuthors();
    size_t count = 1;
    output_ << "Select author:" << std::endl;
    for (const auto& item : list_of_authors) {
        output_ << count++ << " " << item << std::endl;
    }
    output_ << "Enter author # or empty line to cancel" << std::endl;
    return list_of_authors;
}

std::vector<std::string> View::ShowBooksList() {
    auto list_of_books = use_cases_.GetAllBooks();
    size_t count = 1;
    output_ << "Select book:" << std::endl;
    for (const auto& item : list_of_books) {
        output_ << count++ << " " << item << std::endl;
    }
    output_ << "Enter book # or empty line to cancel" << std::endl;
    return list_of_books;
}

std::optional<size_t> View::ChooseAuthor(const std::vector<std::string>& authors) {
    int index = 0;
    do {
        std::string tmp;
        std::getline(input_, tmp);
        boost::algorithm::trim(tmp);
        if (tmp.empty()) {
            return std::nullopt;
        }
        std::stringstream ss(tmp);
        ss >> index;
        if (index <= 0 || index > static_cast<int>(authors.size())) {
            output_ << "Invalid author. Retry attempt." << std::endl;
        }
    } while (index <= 0 || index > static_cast<int>(authors.size()));
    return index;
}

std::optional<size_t> View::ChooseBook(const std::vector<std::string>& books) {
    int index = 0;
    do {
        std::string tmp;
        std::getline(input_, tmp);
        boost::algorithm::trim(tmp);
        if (tmp.empty()) {
            return std::nullopt;
        }
        std::stringstream ss(tmp);
        ss >> index;
        if (index <= 0 || index > static_cast<int>(books.size())) {
            output_ << "Invalid book. Retry attempt." << std::endl;
        }
    } while (index <= 0 || index > static_cast<int>(books.size()));
    return index;
}

}  // namespace ui