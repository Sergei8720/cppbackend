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

std::vector<std::string> UseCasesImpl::GetAllAuthors() {
    auto authors = authors_.GetAllAuthors();
    
    // ГАРАНТИРОВАННАЯ сортировка по имени
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

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({AuthorId::New(), name});
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

std::vector<Author> UseCasesImpl::GetAllAuthorsSorted() {
    auto authors = authors_.GetAllAuthors();
    std::sort(authors.begin(), authors.end(),
        [](const Author& a, const Author& b) {
            return a.GetName() < b.GetName();
        });
    return authors;
}

std::optional<std::string> UseCasesImpl::GetAuthorIdByName(const std::string& name) {
    auto author = authors_.GetAuthorByName(name);
    if (author) {
        return author->GetId().ToString();
    }
    return std::nullopt;
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

// Books
void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, 
                           int year, const std::vector<std::string>& tags) {
    domain::BookId book_id = domain::BookId::New();
    domain::AuthorId author_id_obj = domain::AuthorId::FromString(author_id);
    
    domain::Book book(book_id, author_id_obj, title, year);
    books_.Save(book);
    
    if (!tags.empty()) {
        books_.SaveTags(book_id, tags);
    }
}

std::vector<std::string> UseCasesImpl::GetAllBooks() {
    std::vector<std::string> result;
    auto books = books_.GetAllBooks();
    
    if (books.empty()) return result;
    
    std::unordered_map<std::string, std::string> author_names;
    auto authors = authors_.GetAllAuthors();
    for (const auto& author : authors) {
        author_names[author.GetId().ToString()] = author.GetName();
    }
    
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
    
    std::sort(result.begin(), result.end(),
        [](const BookDetail& a, const BookDetail& b) {
            if (a.year != b.year) return a.year < b.year;
            return a.title < b.title;
        });
    
    return result;
}

void UseCasesImpl::DeleteBook(const std::string& title, const std::string& author_name) {
    auto books = books_.GetBooksByTitle(title);
    for (const auto& book : books) {
        books_.Delete(book.GetId());
    }
}

void UseCasesImpl::DeleteBookByIndex(int book_index, int title_index) {
    auto books = books_.GetAllBooks();
    if (book_index >= 0 && book_index < static_cast<int>(books.size())) {
        books_.Delete(books[book_index].GetId());
    }
}

void UseCasesImpl::EditBook(const std::string& old_title, const std::string& new_title,
                            const std::string& new_year, const std::vector<std::string>& new_tags,
                            const std::string& author_name) {
    auto books = books_.GetBooksByTitle(old_title);
    for (auto& book : books) {
        if (!new_title.empty() && new_title != " ") {
            book.SetTitle(new_title);
        }
        if (!new_year.empty() && new_year != " ") {
            try {
                book.SetPublicationYear(std::stoi(new_year));
            } catch (...) {}
        }
        books_.Update(book);
        
        if (!new_tags.empty()) {
            books_.SaveTags(book.GetId(), new_tags);
        }
    }
}

void UseCasesImpl::EditBookByIndex(int book_index, const std::string& new_title,
                                   const std::string& new_year, const std::vector<std::string>& new_tags) {
    auto books = books_.GetAllBooks();
    if (book_index >= 0 && book_index < static_cast<int>(books.size())) {
        auto book = books[book_index];
        if (!new_title.empty() && new_title != " ") {
            book.SetTitle(new_title);
        }
        if (!new_year.empty() && new_year != " ") {
            try {
                book.SetPublicationYear(std::stoi(new_year));
            } catch (...) {}
        }
        books_.Update(book);
        
        if (!new_tags.empty()) {
            books_.SaveTags(book.GetId(), new_tags);
        }
    }
}

std::vector<std::string> UseCasesImpl::GetBooksByAuthor(const std::string& author_name) {
    std::vector<std::string> result;
    auto books = books_.GetBooksByAuthor(author_name);
    
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