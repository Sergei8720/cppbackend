#include "use_cases_impl.h"
#include "author.h"
#include "book.h"

#include <ranges>
#include <algorithm>
#include <sstream>
#include <optional>

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({AuthorId::New(), name});
}

void UseCasesImpl::DeleteAuthor(const std::string& name) {
    authors_.Delete(name);
}

void UseCasesImpl::EditAuthor(const std::string& old_name, const std::string& new_name) {
    authors_.UpdateName(old_name, new_name);
}

std::vector<std::string> UseCasesImpl::GetAllAuthors(){
    std::vector<std::string> list_of_authors;
    std::ranges::transform(
        authors_.GetAllAuthors(),
        std::back_inserter(list_of_authors),
        [](auto& author) -> std::string {
            return author.GetName();
        }
    );
    return list_of_authors;
}

std::optional<std::string> UseCasesImpl::GetAuthorIdBy(const std::string& author_name) {
    auto author = authors_.GetAuthorBy(author_name);
    if(author) {
        return author->GetId().ToString();
    }
    return std::nullopt;
};

void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, int year, const std::vector<std::string>& tags) {
    auto book_id = BookId::New();
    books_.Save({book_id, AuthorId::FromString(author_id), title, year});
    books_.SaveTags(book_id, tags);
};

void UseCasesImpl::DeleteBook(const std::string& id) {
    auto book = books_.FindById(BookId::FromString(id));
    if (!book) {
        throw std::runtime_error("Book not found");
    }
    books_.DeleteById(BookId::FromString(id));
};

std::vector<BookInfo> UseCasesImpl::GetAllBooks() {
    std::vector<BookInfo> result;
    auto books = books_.GetAllBooks();
    
    for (const auto& book : books) {
        auto author = authors_.GetAuthorBy(book.GetAuthorId());
        if (author) {
            auto tags = books_.GetTags(book.GetId());
            result.push_back({
                book.GetId().ToString(),
                book.GetTitle(),
                author->GetName(),
                book.GetPublicationYear(),
                tags
            });
        }
    }
    
    // Сортировка: по названию, по автору, по году
    std::sort(result.begin(), result.end(),
        [](const BookInfo& a, const BookInfo& b) {
            if (a.title != b.title) return a.title < b.title;
            if (a.author_name != b.author_name) return a.author_name < b.author_name;
            return a.year < b.year;
        });
    
    return result;
};

std::vector<BookInfo> UseCasesImpl::GetBooksByAuthor(const std::string& author_name) {
    std::vector<BookInfo> result;
    auto books = books_.GetBooksBy(author_name);
    
    for (const auto& book : books) {
        auto author = authors_.GetAuthorBy(book.GetAuthorId());
        if (author) {
            auto tags = books_.GetTags(book.GetId());
            result.push_back({
                book.GetId().ToString(),
                book.GetTitle(),
                author->GetName(),
                book.GetPublicationYear(),
                tags
            });
        }
    }
    
    return result;
};

std::vector<BookInfo> UseCasesImpl::FindBooksByTitle(const std::string& title) {
    std::vector<BookInfo> result;
    auto books = books_.FindByTitle(title);
    
    for (const auto& book : books) {
        auto author = authors_.GetAuthorBy(book.GetAuthorId());
        if (author) {
            auto tags = books_.GetTags(book.GetId());
            result.push_back({
                book.GetId().ToString(),
                book.GetTitle(),
                author->GetName(),
                book.GetPublicationYear(),
                tags
            });
        }
    }
    
    return result;
};

std::optional<BookInfo> UseCasesImpl::GetBookById(const std::string& id) {
    auto book = books_.FindById(BookId::FromString(id));
    if (!book) {
        return std::nullopt;
    }
    
    auto author = authors_.GetAuthorBy(book->GetAuthorId());
    if (!author) {
        return std::nullopt;
    }
    
    auto tags = books_.GetTags(book->GetId());
    
    return BookInfo{
        book->GetId().ToString(),
        book->GetTitle(),
        author->GetName(),
        book->GetPublicationYear(),
        tags
    };
};

void UseCasesImpl::EditBook(const std::string& id, const std::string& new_title,
                           const std::optional<int>& new_year, const std::vector<std::string>& new_tags) {
    auto book = books_.FindById(BookId::FromString(id));
    if (!book) {
        throw std::runtime_error("Book not found");
    }
    
    // Обновляем основную информацию
    std::string title_to_use = new_title.empty() ? book->GetTitle() : new_title;
    books_.Update(BookId::FromString(id), title_to_use, new_year);
    
    // Обновляем теги ТОЛЬКО если они были предоставлены
    // В параметризованных тестах new_tags может быть пустым вектором,
    // что означает "не обновлять теги"
    if (!new_tags.empty()) {
        books_.SaveTags(BookId::FromString(id), new_tags);
    }
    // Если new_tags пустой, оставляем текущие теги без изменений
};

}  // namespace app