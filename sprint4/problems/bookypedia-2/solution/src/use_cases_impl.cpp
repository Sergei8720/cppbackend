#include "use_cases_impl.h"
#include "author.h"
#include "book.h"

#include <ranges>
#include <algorithm>
#include <sstream>
#include <optional>
#include <unordered_map>
#include <cctype>

namespace app {
using namespace domain;

std::vector<std::string> UseCasesImpl::GetAllAuthorNames() {
    std::vector<std::string> names;
    auto authors = authors_.GetAllAuthors();
    for (const auto& author : authors) {
        names.push_back(author.GetName());
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<Author> UseCasesImpl::GetAllAuthorsSorted() {
    auto authors = authors_.GetAllAuthors();
    std::sort(authors.begin(), authors.end(),
        [](const Author& a, const Author& b) {
            return a.GetName() < b.GetName();
        });
    return authors;
}

std::vector<std::pair<Book, std::string>> UseCasesImpl::GetAllBooksWithAuthors() {
    std::vector<std::pair<Book, std::string>> result;
    auto books = books_.GetAllBooks();
    
    // Создаем map для быстрого поиска авторов
    std::unordered_map<std::string, std::string> author_names;
    auto authors = authors_.GetAllAuthors();
    for (const auto& author : authors) {
        author_names[author.GetId().ToString()] = author.GetName();
    }
    
    for (const auto& book : books) {
        auto it = author_names.find(book.GetAuthorId().ToString());
        if (it != author_names.end()) {
            result.emplace_back(book, it->second);
        }
    }
    
    // Сортируем по названию книги
    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) {
            return a.first.GetTitle() < b.first.GetTitle();
        });
    
    return result;
}

int UseCasesImpl::FindAuthorIndexByName(const std::string& name) {
    auto names = GetAllAuthorNames();
    auto it = std::find(names.begin(), names.end(), name);
    if (it != names.end()) {
        return std::distance(names.begin(), it);
    }
    return -1;
}

int UseCasesImpl::FindBookIndexByTitle(const std::string& title, const std::string& author_name) {
    auto books = GetAllBooksWithAuthors();
    if (author_name.empty()) {
        // Ищем первую книгу с таким названием
        for (size_t i = 0; i < books.size(); ++i) {
            if (books[i].first.GetTitle() == title) {
                return i;
            }
        }
    } else {
        // Ищем книгу с таким названием и автором
        for (size_t i = 0; i < books.size(); ++i) {
            if (books[i].first.GetTitle() == title && books[i].second == author_name) {
                return i;
            }
        }
    }
    return -1;
}

// Authors
void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({AuthorId::New(), name});
}

std::vector<std::string> UseCasesImpl::GetAllAuthors() {
    auto authors = authors_.GetAllAuthors();
    
    // Сортируем по имени
    std::sort(authors.begin(), authors.end(),
        [](const domain::Author& a, const domain::Author& b) {
            return a.GetName() < b.GetName();
        });
    
    std::vector<std::string> result;
    for (const auto& author : authors) {
        result.push_back(author.GetName());
    }
    return result;
}

void UseCasesImpl::DeleteAuthor(const std::string& name) {
    auto author = authors_.GetAuthorByName(name);
    if (author) {
        authors_.Delete(author->GetId());
    }
}

void UseCasesImpl::DeleteAuthorByIndex(int index) {
    auto authors = GetAllAuthorsSorted();
    if (index >= 0 && index < static_cast<int>(authors.size())) {
        authors_.Delete(authors[index].GetId());
    }
}

void UseCasesImpl::EditAuthor(const std::string& old_name, const std::string& new_name) {
    auto author = authors_.GetAuthorByName(old_name);
    if (author) {
        author->SetName(new_name);
        authors_.Update(*author);
    }
}

void UseCasesImpl::EditAuthorByIndex(int index, const std::string& new_name) {
    auto authors = GetAllAuthorsSorted();
    if (index >= 0 && index < static_cast<int>(authors.size())) {
        auto author = authors[index];
        author.SetName(new_name);
        authors_.Update(author);
    }
}

std::optional<std::string> UseCasesImpl::GetAuthorIdByName(const std::string& name) {
    auto author = authors_.GetAuthorByName(name);
    if (author) {
        return author->GetId().ToString();
    }
    return std::nullopt;
}

// Books
void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, 
                           int year, const std::vector<std::string>& tags) {
    domain::BookId book_id = domain::BookId::New();
    domain::AuthorId author_id_obj = domain::AuthorId::FromString(author_id);
    
    domain::Book book(book_id, author_id_obj, title, year);
    books_.Save(book);
    
    // Сохраняем теги, если они есть
    if (!tags.empty()) {
        // Фильтруем пустые теги
        std::vector<std::string> clean_tags;
        for (const auto& tag : tags) {
            std::string trimmed = tag;
            boost::algorithm::trim(trimmed);
            if (!trimmed.empty()) {
                clean_tags.push_back(trimmed);
            }
        }
        
        if (!clean_tags.empty()) {
            books_.SaveTags(book_id, clean_tags);
        }
    }
}

std::vector<std::string> UseCasesImpl::GetAllBooks() {
    std::vector<std::string> result;
    auto books = books_.GetAllBooks();
    
    if (books.empty()) {
        return result;
    }
    
    // Получаем всех авторов
    std::unordered_map<std::string, std::string> author_names;
    auto authors = authors_.GetAllAuthors();
    for (const auto& author : authors) {
        author_names[author.GetId().ToString()] = author.GetName();
    }
    
    // Сортируем книги по названию
    std::sort(books.begin(), books.end(),
        [](const domain::Book& a, const domain::Book& b) {
            return a.GetTitle() < b.GetTitle();
        });
    
    for (size_t i = 0; i < books.size(); ++i) {
        const auto& book = books[i];
        auto it = author_names.find(book.GetAuthorId().ToString());
        if (it != author_names.end()) {
            std::stringstream ss;
            ss << i + 1 << " " << book.GetTitle() << " by " 
               << it->second << ", " << book.GetPublicationYear();
            result.push_back(ss.str());
        }
    }
    
    return result;
}

std::vector<BookDetail> UseCasesImpl::GetBooksByTitle(const std::string& title) {
    std::vector<BookDetail> result;
    auto books = books_.GetBooksByTitle(title);
    
    for (const auto& book : books) {
        auto author = authors_.GetAuthorById(book.GetAuthorId());
        if (author) {
            BookDetail detail;
            detail.id = book.GetId().ToString();
            detail.title = book.GetTitle();
            detail.author_name = author->GetName();
            detail.year = book.GetPublicationYear();
            detail.tags = books_.GetTags(book.GetId());
            result.push_back(std::move(detail));
        }
    }
    
    // Сортируем по году и названию
    std::sort(result.begin(), result.end(),
        [](const BookDetail& a, const BookDetail& b) {
            if (a.year != b.year) return a.year < b.year;
            return a.title < b.title;
        });
    
    return result;
}

void UseCasesImpl::DeleteBook(const std::string& title, const std::string& author_name) {
    if (author_name.empty()) {
        // Удаляем все книги с таким названием
        auto books = books_.GetBooksByTitle(title);
        for (const auto& book : books) {
            books_.Delete(book.GetId());
        }
    } else {
        int index = FindBookIndexByTitle(title, author_name);
        if (index != -1) {
            auto books = GetAllBooksWithAuthors();
            if (index >= 0 && index < static_cast<int>(books.size())) {
                books_.Delete(books[index].first.GetId());
            }
        }
    }
}

void UseCasesImpl::DeleteBookByIndex(int book_index, int title_index) {
    auto books = GetAllBooksWithAuthors();
    if (book_index >= 0 && book_index < static_cast<int>(books.size())) {
        books_.Delete(books[book_index].first.GetId());
    }
}

void UseCasesImpl::EditBook(const std::string& old_title, const std::string& new_title,
                            const std::string& new_year, const std::vector<std::string>& new_tags,
                            const std::string& author_name) {
    if (author_name.empty()) {
        // Редактируем все книги с таким названием
        auto books = books_.GetBooksByTitle(old_title);
        for (auto& book : books) {
            EditSingleBook(book, new_title, new_year, new_tags);
        }
    } else {
        int index = FindBookIndexByTitle(old_title, author_name);
        if (index != -1) {
            auto books_with_authors = GetAllBooksWithAuthors();
            if (index >= 0 && index < static_cast<int>(books_with_authors.size())) {
                auto book = books_with_authors[index].first;
                EditSingleBook(book, new_title, new_year, new_tags);
            }
        }
    }
}

void UseCasesImpl::EditBookByIndex(int book_index, const std::string& new_title,
                                   const std::string& new_year, const std::vector<std::string>& new_tags) {
    auto books_with_authors = GetAllBooksWithAuthors();
    if (book_index >= 0 && book_index < static_cast<int>(books_with_authors.size())) {
        auto book = books_with_authors[book_index].first;
        EditSingleBook(book, new_title, new_year, new_tags);
    }
}

void UseCasesImpl::EditSingleBook(domain::Book& book, const std::string& new_title,
                                  const std::string& new_year, const std::vector<std::string>& new_tags) {
    bool updated = false;
    
    if (!new_title.empty() && new_title != " ") {
        book.SetTitle(new_title);
        updated = true;
    }
    
    if (!new_year.empty() && new_year != " ") {
        try {
            int year = std::stoi(new_year);
            book.SetPublicationYear(year);
            updated = true;
        } catch (...) {}
    }
    
    if (updated) {
        books_.Update(book);
    }
    
    if (!new_tags.empty()) {
        std::vector<std::string> tags_to_save;
        for (const auto& tag : new_tags) {
            std::string trimmed = tag;
            boost::algorithm::trim(trimmed);
            if (!trimmed.empty()) {
                tags_to_save.push_back(trimmed);
            }
        }
        if (!tags_to_save.empty()) {
            books_.SaveTags(book.GetId(), tags_to_save);
        }
    }
}

std::vector<std::string> UseCasesImpl::GetBooksByAuthor(const std::string& author_name) {
    std::vector<std::string> result;
    auto books = books_.GetBooksByAuthor(author_name);
    
    // Сортируем по году и названию
    std::sort(books.begin(), books.end(),
        [](const Book& a, const Book& b) {
            if (a.GetPublicationYear() != b.GetPublicationYear()) {
                return a.GetPublicationYear() < b.GetPublicationYear();
            }
            return a.GetTitle() < b.GetTitle();
        });
    
    for (size_t i = 0; i < books.size(); ++i) {
        std::stringstream ss;
        ss << i + 1 << " " << books[i].GetTitle() << ", " << books[i].GetPublicationYear();
        result.push_back(ss.str());
    }
    
    return result;
}

}  // namespace app