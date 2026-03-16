#pragma once
#include <string>
#include <vector>

#include "tagged_uuid.h"
#include "author.h"

namespace domain {

namespace detail {
struct BookTag {};
}  // namespace detail

using BookId = util::TaggedUUID<detail::BookTag>;

class Book {
public:
    Book(BookId id, AuthorId author_id, std::string title, int year)
        : id_(std::move(id))
        , author_id_(std::move(author_id))
        , title_(std::move(title))
        , publication_year_(year) {
    }

    const BookId& GetId() const noexcept {
        return id_;
    }

    const AuthorId& GetAuthorId() const noexcept {
        return author_id_;
    }

    const std::string& GetTitle() const noexcept {
        return title_;
    }

    int GetPublicationYear() const noexcept {
        return publication_year_;
    }

    void SetTitle(const std::string& title) {
        title_ = title;
    }

    void SetPublicationYear(int year) {
        publication_year_ = year;
    }

private:
    BookId id_;
    AuthorId author_id_;
    std::string title_;
    int publication_year_;
};

struct BookInfo {
    BookId id;
    std::string title;
    std::string author_name;
    int year;
    std::vector<std::string> tags;
};

class BookRepository {
public:
    virtual void Save(const Book& book) = 0;
    virtual void Delete(const BookId& id) = 0;
    virtual void Update(const Book& book) = 0;
    virtual std::vector<Book> GetAllBooks() = 0;
    virtual std::vector<Book> GetBooksByAuthor(const std::string& author_name) = 0;
    virtual std::vector<Book> GetBooksByTitle(const std::string& title) = 0;
    virtual void SaveTags(const BookId& book_id, const std::vector<std::string>& tags) = 0;
    virtual std::vector<std::string> GetTags(const BookId& book_id) = 0;

protected:
    ~BookRepository() = default;
};

}  // namespace domain