#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <set>
#include <regex>

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
                return true;
            }
            auto index = ChooseAuthor(list_of_authors);
            if (!index) {
                return true;  // Отмена - ничего не выводим
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
                return true;
            }
            auto index = ChooseAuthor(list_of_authors);
            if (!index) {
                return true;  // Отмена - ничего не выводим
            }
            old_name = list_of_authors[*index - 1];
        }
        
        if (!use_cases_.GetAuthorIdBy(old_name)) {
            output_ << "Failed to edit author"sv << std::endl;
            return true;
        }
        
        output_ << "Enter new name:"sv << std::endl;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);
        
        if (new_name.empty()) {
            output_ << "Failed to edit author"sv << std::endl;
            return true;
        }
        
        use_cases_.EditAuthor(old_name, new_name);
    } catch (const std::exception&) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}

std::vector<std::string> View::NormalizeTags(const std::string& tags_input) {
    std::set<std::string> unique_tags;
    std::vector<std::string> raw_tags;
    boost::algorithm::split(raw_tags, tags_input, boost::algorithm::is_any_of(","));
    
    for (auto& tag : raw_tags) {
        // Удаляем пробелы в начале и конце
        boost::algorithm::trim(tag);
        if (tag.empty()) continue;
        
        // Заменяем множественные пробелы на один
        std::regex multiple_spaces("\\s+");
        std::string normalized = std::regex_replace(tag, multiple_spaces, " ");
        
        // Удаляем пробелы в начале и конце после нормализации
        boost::algorithm::trim(normalized);
        
        if (!normalized.empty()) {
            unique_tags.insert(normalized);
        }
    }
    
    return std::vector<std::string>(unique_tags.begin(), unique_tags.end());
}

bool View::AddBook(std::istream& cmd_input) {
    try {
        int year = 0;
        std::string title;
        
        cmd_input >> year;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        
        if (cmd_input.eof()) {
            cmd_input.clear();
        }

        std::string author_name;
        std::optional<std::string> author_id;
        
        output_ << "Enter author name or empty line to select from list:" << std::endl;
        std::getline(input_, author_name);
        boost::algorithm::trim(author_name);
        
        if (author_name.empty()) {
            // Выбор из списка
            auto authors = use_cases_.GetAllAuthors();
            if (authors.empty()) {
                return true;
            }
            
            auto list_of_authors = ShowAuthorsList();
            auto index = ChooseAuthor(list_of_authors);
            if (!index) {
                return true;
            }
            
            author_name = list_of_authors[*index - 1];
            author_id = use_cases_.GetAuthorIdBy(author_name);
        } else {
            // Поиск автора по имени
            author_id = use_cases_.GetAuthorIdBy(author_name);
            
            if (!author_id) {
                output_ << "No author found. Do you want to add " << author_name << " (y/n)?" << std::endl;
                std::string answer;
                std::getline(input_, answer);
                boost::algorithm::trim(answer);
                
                if (answer != "y" && answer != "Y") {
                    output_ << "Failed to add book"sv << std::endl;
                    return true;
                }
                
                use_cases_.AddAuthor(author_name);
                author_id = use_cases_.GetAuthorIdBy(author_name);
            }
        }
        
        if (!author_id) {
            output_ << "Failed to add book"sv << std::endl;
            return true;
        }
        
        output_ << "Enter tags (comma separated):" << std::endl;
        std::string tags_input;
        std::getline(input_, tags_input);
        
        auto tags = NormalizeTags(tags_input);
        use_cases_.AddBook(*author_id, title, year, tags);
        
    } catch (const std::exception& e) {
        output_ << "Failed to add book: " << e.what() << std::endl;
    }
    return true;
}

std::string View::GetCurrentTagsString(const std::vector<std::string>& tags) {
    if (tags.empty()) return "";
    std::string result;
    for (const auto& tag : tags) {
        if (!result.empty()) result += ", ";
        result += tag;
    }
    return result;
}

bool View::ShowBooks() {
    try {
        size_t count = 1;
        auto books = use_cases_.GetAllBooks();
        for (const auto& book : books) {
            output_ << count++ << " " << book.title << " by " << book.author_name << ", " << book.year << std::endl;
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
        auto books = use_cases_.GetBooksByAuthor(list_of_authors[*index - 1]);
        size_t count = 1;
        for (const auto& book : books) {
            output_ << count++ << " " << book.title << ", " << book.year << std::endl;
        }
    } catch (const std::exception& e) {
        output_ << "Failed to show books: " << e.what() << std::endl;
    }
    return true;
}

std::optional<app::BookInfo> View::SelectBookFromList(const std::vector<app::BookInfo>& books, const std::string& prompt) {
    if (books.empty()) {
        return std::nullopt;
    }
    
    if (books.size() == 1) {
        return books[0];
    }
    
    while (true) {
        size_t count = 1;
        for (const auto& book : books) {
            output_ << count++ << " " << book.title << " by " << book.author_name << ", " << book.year << std::endl;
        }
        output_ << prompt << std::endl;
        
        std::string input;
        std::getline(input_, input);
        boost::algorithm::trim(input);
        
        if (input.empty()) {
            return std::nullopt;
        }
        
        try {
            size_t index = std::stoul(input);
            if (index >= 1 && index <= books.size()) {
                return books[index - 1];
            }
        } catch (...) {
            // ignore
        }
        
        output_ << "Invalid book. Retry attempt." << std::endl;
    }
}

bool View::ShowBook(std::istream& cmd_input) {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        
        std::vector<app::BookInfo> books;
        
        if (title.empty()) {
            // Показать все книги для выбора
            books = use_cases_.GetAllBooks();
        } else {
            // Поиск по названию
            books = use_cases_.FindBooksByTitle(title);
        }
        
        if (books.empty()) {
            return true;  // Ничего не выводим
        }
        
        auto selected = SelectBookFromList(books, "Enter the book # or empty line to cancel:");
        if (!selected) {
            return true;
        }
        
        output_ << "Title: " << selected->title << std::endl;
        output_ << "Author: " << selected->author_name << std::endl;
        output_ << "Publication year: " << selected->year << std::endl;
        
        if (!selected->tags.empty()) {
            output_ << "Tags: " << GetCurrentTagsString(selected->tags) << std::endl;
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
        
        std::vector<app::BookInfo> books;
        
        if (title.empty()) {
            books = use_cases_.GetAllBooks();
        } else {
            books = use_cases_.FindBooksByTitle(title);
        }
        
        if (books.empty()) {
            return true;  // Ничего не выводим
        }
        
        auto selected = SelectBookFromList(books, "Enter the book # or empty line to cancel:");
        if (!selected) {
            return true;  // Отмена - ничего не выводим
        }
        
        use_cases_.DeleteBook(selected->id);
        
    } catch (const std::exception& e) {
        output_ << "Failed to delete book" << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        
        std::vector<app::BookInfo> books;
        
        if (title.empty()) {
            books = use_cases_.GetAllBooks();
        } else {
            books = use_cases_.FindBooksByTitle(title);
        }
        
        if (books.empty()) {
            output_ << "Book not found" << std::endl;
            return true;
        }
        
        auto selected = SelectBookFromList(books, "Enter the book # or empty line to cancel:");
        if (!selected) {
            output_ << "Book not found" << std::endl;  // При отмене выводим "Book not found"
            return true;
        }
        
        output_ << "Enter new title or empty line to use the current one (" << selected->title << "):" << std::endl;
        std::string new_title;
        std::getline(input_, new_title);
        boost::algorithm::trim(new_title);
        
        output_ << "Enter publication year or empty line to use the current one (" << selected->year << "):" << std::endl;
        std::string year_str;
        std::getline(input_, year_str);
        boost::algorithm::trim(year_str);
        std::optional<int> new_year;
        if (!year_str.empty()) {
            try {
                new_year = std::stoi(year_str);
            } catch (...) {
            }
        }
        
        std::string current_tags = GetCurrentTagsString(selected->tags);
        output_ << "Enter tags (current tags: " << (current_tags.empty() ? "none" : current_tags) << "):" << std::endl;
        std::string tags_input;
        std::getline(input_, tags_input);
        
        std::vector<std::string> new_tags;
        if (!tags_input.empty()) {
            new_tags = NormalizeTags(tags_input);
        }
        // Если tags_input пустой, new_tags остается пустым, и теги не обновляются
        
        use_cases_.EditBook(selected->id, new_title, new_year, new_tags);
        
    } catch (const std::exception& e) {
        output_ << "Failed to edit book" << std::endl;
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
    auto books = use_cases_.GetAllBooks();
    std::vector<std::string> result;
    size_t count = 1;
    output_ << "Select book:" << std::endl;
    for (const auto& book : books) {
        std::string line = std::to_string(count++) + " " + book.title + " by " + book.author_name + ", " + std::to_string(book.year);
        output_ << line << std::endl;
        result.push_back(line);
    }
    output_ << "Enter book # or empty line to cancel" << std::endl;
    return result;
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