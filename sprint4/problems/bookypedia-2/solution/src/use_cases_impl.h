#pragma once
#include "author_fwd.h"
#include "book_fwd.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(domain::AuthorRepository& authors,
                        domain::BookRepository& books)
        : authors_{authors}
        , books_{books} {
    }

    void AddAuthor(const std::string& name) override;
    std::vector<std::string> GetAllAuthors() override;
    std::optional<std::string> GetAuthorIdBy(const std::string& author_name) override;
    void DeleteAuthor(const std::string& name) override;
    void EditAuthor(const std::string& old_name, const std::string& new_name) override;
    
    void AddBook(const std::string& author_id, const std::string& title, int year, const std::vector<std::string>& tags) override;
    std::vector<BookInfo> GetAllBooks() override;
    std::vector<BookInfo> GetBooksByAuthor(const std::string& author_name) override;
    std::vector<BookInfo> FindBooksByTitle(const std::string& title) override;
    std::optional<BookInfo> GetBookById(const std::string& id) override;
    void DeleteBook(const std::string& id) override;
    void EditBook(const std::string& id, const std::string& new_title,
                 const std::optional<int>& new_year, const std::vector<std::string>& new_tags) override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app