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

void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, int year) {
    books_.Save({BookId::New(), AuthorId::FromString(author_id), title, year});
};

void UseCasesImpl::DeleteBook(const std::string& title) {
    books_.Delete(title);
}

std::vector<std::string> UseCasesImpl::ShowBook(const std::string& title) {
    std::vector<std::string> result;
    auto books = books_.FindByTitle(title);
    if (books.empty()) {
        result.push_back("Book not found");
        return result;
    }
    
    for (const auto& book : books) {
        auto author = authors_.GetAuthorBy(book.GetAuthorId().ToString());
        if (author) {
            std::stringstream ss;
            ss << "Title: " << book.GetTitle() << std::endl;
            ss << "Author: " << author->GetName() << std::endl;
            ss << "Publication year: " << book.GetPublicationYear();
            result.push_back(ss.str());
        }
    }
    return result;
}

void UseCasesImpl::EditBook(const std::string& old_title, const std::string& new_title,
                           const std::optional<int>& new_year, const std::optional<std::string>& new_tags) {
    books_.Update(old_title, new_title, new_year, new_tags);
}

std::vector<std::string> UseCasesImpl::GetAllBooks() {
    std::vector<std::string> list_of_books;
    std::ranges::transform(
        books_.GetAllBooks(),
        std::back_inserter(list_of_books),
        [this](auto& book) -> std::string {
            auto author = authors_.GetAuthorBy(book.GetAuthorId().ToString());
            std::stringstream ss;
            if (author) {
                ss << book.GetTitle() << " by " << author->GetName() << ", " << book.GetPublicationYear();
            } else {
                ss << book.GetTitle() << ", " << book.GetPublicationYear();
            }
            return ss.str();
        }
    );
    return list_of_books;
};

std::vector<std::string> UseCasesImpl::GetBooksBy(const std::string& author_name) {
    std::vector<std::string> list_of_books;
    std::ranges::transform(
        books_.GetBooksBy(author_name),
        std::back_inserter(list_of_books),
        [](auto& book) -> std::string {
            std::stringstream ss;
            ss << book.GetTitle() << ", " << book.GetPublicationYear();
            return ss.str();
        }
    );
    return list_of_books;
};

}  // namespace app