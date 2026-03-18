#pragma once

#include <string>
#include <vector>
#include <optional>

namespace app {

struct BookInfo {
    std::string id;
    std::string title;
    std::string author_name;
    int year;
    std::vector<std::string> tags;
};

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual std::vector<std::string> GetAllAuthors() = 0;
    virtual std::optional<std::string> GetAuthorIdBy(const std::string& author_name) = 0;
    virtual void DeleteAuthor(const std::string& name) = 0;
    virtual void EditAuthor(const std::string& old_name, const std::string& new_name) = 0;
    
    virtual void AddBook(const std::string& author_id, const std::string& title, int year, const std::vector<std::string>& tags) = 0;
    virtual std::vector<BookInfo> GetAllBooks() = 0;
    virtual std::vector<BookInfo> GetBooksByAuthor(const std::string& author_name) = 0;
    virtual std::vector<BookInfo> FindBooksByTitle(const std::string& title) = 0;
    virtual std::optional<BookInfo> GetBookById(const std::string& id) = 0;
    virtual void DeleteBook(const std::string& id) = 0;
    virtual void EditBook(const std::string& id, const std::string& new_title,
                         const std::optional<int>& new_year, const std::vector<std::string>& new_tags) = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app