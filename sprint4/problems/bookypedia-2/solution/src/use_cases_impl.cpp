#include "use_cases_impl.h"
#include "author.h"
#include "book.h"

#include <ranges>
#include <algorithm>
#include <sstream>
#include <optional>
#include <unordered_map>

namespace app {
using namespace domain;

std::vector<std::string> UseCasesImpl::GetAllAuthorNames() {
    std::vector<std::string> names;
    auto authors = authors_.GetAllAuthors();
    for(const auto& author : authors) {
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
    for(const auto& author : authors) {
        author_names[author.GetId().ToString()] = author.GetName();
    }
    
    for(const auto& book : books) {
        result.emplace_back(book, author_names[book.GetAuthorId().ToString()]);
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
    if(it != names.end()) {
        return std::distance(names.begin(), it);
    }
    return -1;
}

int UseCasesImpl::FindBookIndexByTitle(const std::string& title, const std::string& author_name) {
    auto books = GetAllBooksWithAuthors();
    if(author_name.empty()) {
        // Ищем первую книгу с таким названием
        for(size_t i = 0; i < books.size(); ++i) {
            if(books[i].first.GetTitle() == title) {
                return i;
            }
        }
    } else {
        // Ищем книгу с таким названием и автором
        for(size_t i = 0; i < books.size(); ++i) {
            if(books[i].first.GetTitle() == title && books[i].second == author_name) {
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
    return GetAllAuthorNames();
}

void UseCasesImpl::DeleteAuthor(const std::string& name) {
    auto author = authors_.GetAuthorByName(name);
    if(author) {
        authors_.Delete(author->GetId());
    }
}

void UseCasesImpl::DeleteAuthorByIndex(int index) {
    auto authors = GetAllAuthorsSorted();
    if(index >= 0 && index < static_cast<int>(authors.size())) {
        authors_.Delete(authors[index].GetId());
    }
}

void UseCasesImpl::EditAuthor(const std::string& old_name, const std::string& new_name) {
    auto author = authors_.GetAuthorByName(old_name);
    if(author) {
        author->SetName(new_name);
        authors_.Update(*author);
    }
}

void UseCasesImpl::EditAuthorByIndex(int index, const std::string& new_name) {
    auto authors = GetAllAuthorsSorted();
    if(index >= 0 && index < static_cast<int>(authors.size())) {
        auto author = authors[index];
        author.SetName(new_name);
        authors_.Update(author);
    }
}

std::optional<std::string> UseCasesImpl::GetAuthorIdByName(const std::string& name) {
    auto author = authors_.GetAuthorByName(name);
    if(author) {
        return author->GetId().ToString();
    }
    return std::nullopt;
}

// Books
void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, 
                           int year, const std::vector<std::string>& tags) {
    BookId book_id = BookId::New();
    books_.Save({book_id, AuthorId::FromString(author_id), title, year});
    books_.SaveTags(book_id, tags);
}

std::vector<std::string> UseCasesImpl::GetAllBooks() {
    std::vector<std::string> result;
    auto books_with_authors = GetAllBooksWithAuthors();
    
    for(size_t i = 0; i < books_with_authors.size(); ++i) {
        std::stringstream ss;
        ss << i+1 << " " << books_with_authors[i].first.GetTitle() 
           << " by " << books_with_authors[i].second << ", " 
           << books_with_authors[i].first.GetPublicationYear();
        result.push_back(ss.str());
    }
    
    return result;
}

std::vector<BookDetail> UseCasesImpl::GetBooksByTitle(const std::string& title) {
    std::vector<BookDetail> result;
    auto books = books_.GetBooksByTitle(title);
    
    for(const auto& book : books) {
        auto author = authors_.GetAuthorById(book.GetAuthorId());
        if(author) {
            BookDetail detail;
            detail.id = book.GetId().ToString();
            detail.title = book.GetTitle();
            detail.author_name = author->GetName();
            detail.year = book.GetPublicationYear();
            detail.tags = books_.GetTags(book.GetId());
            result.push_back(detail);
        }
    }
    
    // Сортируем по году и названию
    std::sort(result.begin(), result.end(),
        [](const BookDetail& a, const BookDetail& b) {
            if(a.year != b.year) return a.year < b.year;
            return a.title < b.title;
        });
    
    return result;
}

void UseCasesImpl::DeleteBook(const std::string& title, const std::string& author_name) {
    int index = FindBookIndexByTitle(title, author_name);
    if(index != -1) {
        auto books = GetAllBooksWithAuthors();
        books_.Delete(books[index].first.GetId());
    }
}

void UseCasesImpl::DeleteBookByIndex(int book_index, int title_index) {
    auto books = GetAllBooksWithAuthors();
    if(title_index != -1) {
        // Удаляем конкретную книгу с таким названием
        std::vector<std::pair<Book, std::string>> books_with_title;
        for(const auto& book : books) {
            if(book.first.GetTitle() == books[book_index].first.GetTitle()) {
                books_with_title.push_back(book);
            }
        }
        if(title_index >= 0 && title_index < static_cast<int>(books_with_title.size())) {
            books_.Delete(books_with_title[title_index].first.GetId());
        }
    } else if(book_index >= 0 && book_index < static_cast<int>(books.size())) {
        books_.Delete(books[book_index].first.GetId());
    }
}

void UseCasesImpl::EditBook(const std::string& old_title, const std::string& new_title,
                            const std::string& new_year, const std::vector<std::string>& new_tags,
                            const std::string& author_name) {
    int index = FindBookIndexByTitle(old_title, author_name);
    if(index != -1) {
        auto books_with_authors = GetAllBooksWithAuthors();
        auto book = books_with_authors[index].first;
        
        if(!new_title.empty() && new_title != " ") {
            book.SetTitle(new_title);
        }
        if(!new_year.empty() && new_year != " ") {
            try {
                book.SetPublicationYear(std::stoi(new_year));
            } catch(...) {}
        }
        
        books_.Update(book);
        
        if(!new_tags.empty()) {
            std::vector<std::string> tags_to_save;
            for(const auto& tag : new_tags) {
                if(!tag.empty()) {
                    tags_to_save.push_back(tag);
                }
            }
            if(!tags_to_save.empty()) {
                books_.SaveTags(book.GetId(), tags_to_save);
            }
        }
    }
}

void UseCasesImpl::EditBookByIndex(int book_index, const std::string& new_title,
                                   const std::string& new_year, const std::vector<std::string>& new_tags) {
    auto books_with_authors = GetAllBooksWithAuthors();
    if(book_index >= 0 && book_index < static_cast<int>(books_with_authors.size())) {
        auto book = books_with_authors[book_index].first;
        
        if(!new_title.empty() && new_title != " ") {
            book.SetTitle(new_title);
        }
        if(!new_year.empty() && new_year != " ") {
            try {
                book.SetPublicationYear(std::stoi(new_year));
            } catch(...) {}
        }
        
        books_.Update(book);
        
        if(!new_tags.empty()) {
            std::vector<std::string> tags_to_save;
            for(const auto& tag : new_tags) {
                if(!tag.empty()) {
                    tags_to_save.push_back(tag);
                }
            }
            if(!tags_to_save.empty()) {
                books_.SaveTags(book.GetId(), tags_to_save);
            }
        }
    }
}

std::vector<std::string> UseCasesImpl::GetBooksByAuthor(const std::string& author_name) {
    std::vector<std::string> result;
    auto books = books_.GetBooksByAuthor(author_name);
    
    // Сортируем по году и названию
    std::sort(books.begin(), books.end(),
        [](const Book& a, const Book& b) {
            if(a.GetPublicationYear() != b.GetPublicationYear()) {
                return a.GetPublicationYear() < b.GetPublicationYear();
            }
            return a.GetTitle() < b.GetTitle();
        });
    
    for(size_t i = 0; i < books.size(); ++i) {
        std::stringstream ss;
        ss << i+1 << " " << books[i].GetTitle() << ", " << books[i].GetPublicationYear();
        result.push_back(ss.str());
    }
    
    return result;
}

}  // namespace app