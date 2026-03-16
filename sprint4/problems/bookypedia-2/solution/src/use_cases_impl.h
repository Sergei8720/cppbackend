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

    // Authors
    void AddAuthor(const std::string& name) override;
    std::vector<std::string> GetAllAuthors() override;
    void DeleteAuthor(const std::string& name) override;
    void DeleteAuthorByIndex(int index) override;
    void EditAuthor(const std::string& old_name, const std::string& new_name) override;
    void EditAuthorByIndex(int index, const std::string& new_name) override;
    std::optional<std::string> GetAuthorIdByName(const std::string& name) override;

    // Books
    void AddBook(const std::string& author_id, const std::string& title, 
                 int year, const std::vector<std::string>& tags) override;
    std::vector<std::string> GetAllBooks() override;
    std::vector<BookDetail> GetBooksByTitle(const std::string& title) override;
    void DeleteBook(const std::string& title, const std::string& author_name) override;
    void DeleteBookByIndex(int book_index, int title_index) override;
    void EditBook(const std::string& old_title, const std::string& new_title,
                  const std::string& new_year, const std::vector<std::string>& new_tags,
                  const std::string& author_name) override;
    void EditBookByIndex(int book_index, const std::string& new_title,
                         const std::string& new_year, const std::vector<std::string>& new_tags) override;
    std::vector<std::string> GetBooksByAuthor(const std::string& author_name) override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
    
    std::vector<std::string> GetAllAuthorNames();
    std::vector<domain::Author> GetAllAuthorsSorted();
    std::vector<std::pair<domain::Book, std::string>> GetAllBooksWithAuthors();
    int FindAuthorIndexByName(const std::string& name);
    int FindBookIndexByTitle(const std::string& title, const std::string& author_name = "");
};

}  // namespace app