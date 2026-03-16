#pragma once

#include <string>
#include <vector>
#include <optional>

namespace app {

struct BookDetail {
    std::string id;
    std::string title;
    std::string author_name;
    int year;
    std::vector<std::string> tags;
};

class UseCases {
public:
    virtual ~UseCases() = default;

    // Authors
    virtual void AddAuthor(const std::string& name) = 0;
    virtual std::vector<std::string> GetAllAuthors() = 0;
    virtual void DeleteAuthor(const std::string& name) = 0;
    virtual void DeleteAuthorByIndex(int index) = 0;
    virtual void EditAuthor(const std::string& old_name, const std::string& new_name) = 0;
    virtual void EditAuthorByIndex(int index, const std::string& new_name) = 0;
    virtual std::optional<std::string> GetAuthorIdByName(const std::string& name) = 0;

    // Books
    virtual void AddBook(const std::string& author_id, const std::string& title, 
                         int year, const std::vector<std::string>& tags) = 0;
    virtual std::vector<std::string> GetAllBooks() = 0;
    virtual std::vector<BookDetail> GetBooksByTitle(const std::string& title) = 0;
    virtual void DeleteBook(const std::string& title, const std::string& author_name = "") = 0;
    virtual void DeleteBookByIndex(int book_index, int title_index = -1) = 0;
    virtual void EditBook(const std::string& old_title, const std::string& new_title,
                          const std::string& new_year, const std::vector<std::string>& new_tags,
                          const std::string& author_name = "") = 0;
    virtual void EditBookByIndex(int book_index, const std::string& new_title,
                                 const std::string& new_year, const std::vector<std::string>& new_tags) = 0;
    virtual std::vector<std::string> GetBooksByAuthor(const std::string& author_name) = 0;
};

}  // namespace app