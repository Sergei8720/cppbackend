#include "postgres.h"

#include <string>
#include <sstream>
#include <pqxx/zview.hxx>
#include <pqxx/pqxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work work_{connection_};
    work_.exec_params(R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
                      author.GetId().ToString(), author.GetName());
    work_.commit();
}

void AuthorRepositoryImpl::Delete(const std::string& name) {
    pqxx::work work_{connection_};
    try {
        // Сначала удаляем теги книг автора
        work_.exec_params(R"(
            DELETE FROM book_tags 
            WHERE book_id IN (
                SELECT id FROM books 
                WHERE author_id = (SELECT id FROM authors WHERE name = $1)
            )
        )"_zv, name);
        
        // Затем удаляем книги автора
        work_.exec_params("DELETE FROM books WHERE author_id = (SELECT id FROM authors WHERE name = $1)"_zv, name);
        
        // Наконец удаляем автора
        work_.exec_params("DELETE FROM authors WHERE name = $1"_zv, name);
        
        work_.commit();
    } catch (const std::exception& e) {
        work_.abort();
        throw;
    }
}

void AuthorRepositoryImpl::UpdateName(const std::string& old_name, const std::string& new_name) {
    pqxx::work work_{connection_};
    work_.exec_params("UPDATE authors SET name=$1 WHERE name=$2"_zv, new_name, old_name);
    work_.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAllAuthors() {
    std::vector<domain::Author> authors;
    pqxx::read_transaction read_transaction_{connection_};
    auto query_text = "SELECT * FROM authors ORDER BY name ASC"_zv;
    for (auto [id, name] : read_transaction_.query<std::string, std::string>(query_text)) {
        authors.emplace_back(domain::AuthorId::FromString(id), name);
    }
    return authors;
}

std::optional<domain::Author> AuthorRepositoryImpl::GetAuthorBy(const std::string& author_name) {
    pqxx::read_transaction read_transaction_{connection_};
    auto query_text = "SELECT * FROM authors WHERE name=" + read_transaction_.quote(author_name);
    auto tmp_author = read_transaction_.query01<std::string, std::string>(query_text);
    if (tmp_author) {
        auto [id, name] = *tmp_author;
        return domain::Author(domain::AuthorId::FromString(id), name);
    };
    return std::nullopt;
};

std::optional<domain::Author> AuthorRepositoryImpl::GetAuthorBy(const domain::AuthorId& id) {
    pqxx::read_transaction read_transaction_{connection_};
    auto query_text = "SELECT * FROM authors WHERE id=" + read_transaction_.quote(id.ToString());
    auto tmp_author = read_transaction_.query01<std::string, std::string>(query_text);
    if (tmp_author) {
        auto [id_str, name] = *tmp_author;
        return domain::Author(domain::AuthorId::FromString(id_str), name);
    };
    return std::nullopt;
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work_{connection_};
    
    work_.exec_params(R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET author_id=$2, title=$3, publication_year=$4;
)"_zv,
                      book.GetId().ToString(),
                      book.GetAuthorId().ToString(),
                      book.GetTitle(),
                      book.GetPublicationYear());
    
    work_.commit();
}

void BookRepositoryImpl::Delete(const std::string& title) {
    pqxx::work work_{connection_};
    try {
        // Сначала удаляем теги книги
        work_.exec_params(R"(
            DELETE FROM book_tags 
            WHERE book_id IN (
                SELECT id FROM books WHERE title = $1
            )
        )"_zv, title);
        
        // Затем удаляем книгу
        work_.exec_params("DELETE FROM books WHERE title = $1"_zv, title);
        
        work_.commit();
    } catch (const std::exception& e) {
        work_.abort();
        throw;
    }
}

void BookRepositoryImpl::DeleteById(const domain::BookId& id) {
    pqxx::work work_{connection_};
    try {
        work_.exec_params("DELETE FROM book_tags WHERE book_id = $1"_zv, id.ToString());
        work_.exec_params("DELETE FROM books WHERE id = $1"_zv, id.ToString());
        work_.commit();
    } catch (const std::exception& e) {
        work_.abort();
        throw;
    }
}

std::vector<domain::Book> BookRepositoryImpl::FindByTitle(const std::string& title) {
    std::vector<domain::Book> books;
    pqxx::read_transaction read_transaction_{connection_};
    auto query_text = "SELECT id, author_id, title, publication_year FROM books WHERE title=" 
        + read_transaction_.quote(title) 
        + " ORDER BY publication_year ASC, title ASC";
    for (auto [id, author_id, title_, year] : read_transaction_.query<std::string, std::string, std::string, int>(query_text)) {
        books.emplace_back(
            domain::BookId::FromString(id),
            domain::AuthorId::FromString(author_id),
            title_,
            year
        );
    }
    return books;
}

std::optional<domain::Book> BookRepositoryImpl::FindById(const domain::BookId& id) {
    pqxx::read_transaction read_transaction_{connection_};
    auto query_text = "SELECT id, author_id, title, publication_year FROM books WHERE id=" 
        + read_transaction_.quote(id.ToString());
    auto result = read_transaction_.query01<std::string, std::string, std::string, int>(query_text);
    if (result) {
        auto [id_str, author_id, title, year] = *result;
        return domain::Book(
            domain::BookId::FromString(id_str),
            domain::AuthorId::FromString(author_id),
            title,
            year
        );
    }
    return std::nullopt;
}

void BookRepositoryImpl::Update(const domain::BookId& id, const std::string& new_title,
                                const std::optional<int>& new_year) {
    pqxx::work work_{connection_};
    
    try {
        if (new_year.has_value()) {
            work_.exec_params("UPDATE books SET title=$1, publication_year=$2 WHERE id=$3",
                            new_title, *new_year, id.ToString());
        } else {
            work_.exec_params("UPDATE books SET title=$1 WHERE id=$2",
                            new_title, id.ToString());
        }
        work_.commit();
    } catch (const std::exception& e) {
        work_.abort();
        throw;
    }
}

void BookRepositoryImpl::SaveTags(const domain::BookId& book_id, const std::vector<std::string>& tags) {
    pqxx::work work_{connection_};
    
    try {
        // Удаляем старые теги
        work_.exec_params("DELETE FROM book_tags WHERE book_id=$1"_zv, book_id.ToString());
        
        // Добавляем новые теги
        for (const auto& tag : tags) {
            if (!tag.empty()) {
                work_.exec_params("INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)"_zv,
                                book_id.ToString(), tag);
            }
        }
        
        work_.commit();
    } catch (const std::exception& e) {
        work_.abort();
        throw;
    }
}

std::vector<std::string> BookRepositoryImpl::GetTags(const domain::BookId& book_id) {
    std::vector<std::string> tags;
    pqxx::read_transaction read_transaction_{connection_};
    auto query_text = "SELECT tag FROM book_tags WHERE book_id=" + read_transaction_.quote(book_id.ToString()) + " ORDER BY tag ASC";
    for (auto [tag] : read_transaction_.query<std::string>(query_text)) {
        tags.push_back(tag);
    }
    return tags;
}

std::vector<domain::Book> BookRepositoryImpl::GetAllBooks() {
    std::vector<domain::Book> books;
    pqxx::read_transaction read_transaction_{connection_};
    auto query_text = "SELECT id, author_id, title, publication_year FROM books ORDER BY title ASC, author_id ASC, publication_year ASC"_zv;
    for (auto [id, author_id, title, year] : read_transaction_.query<std::string, std::string, std::string, int>(query_text)) {
        books.emplace_back(
            domain::BookId::FromString(id),
            domain::AuthorId::FromString(author_id),
            title,
            year
        );
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetBooksBy(const std::string& author_name) {
    std::vector<domain::Book> books;
    pqxx::read_transaction read_transaction_{connection_};
    auto query_text = "SELECT id, author_id, title, publication_year FROM books WHERE author_id=(SELECT id FROM authors WHERE name="
        + read_transaction_.quote(author_name)
        + " LIMIT 1) ORDER BY publication_year ASC, title ASC";
    for (auto [id, author_id, title, year] : read_transaction_.query<std::string, std::string, std::string, int>(query_text)) {
        books.emplace_back(
            domain::BookId::FromString(id),
            domain::AuthorId::FromString(author_id),
            title,
            year
        );
    }
    return books;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work_{connection_};
    work_.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);

    work_.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL,
    title varchar(100) NOT NULL,
    publication_year int NOT NULL,
    FOREIGN KEY (author_id) REFERENCES authors(id) ON DELETE CASCADE
);
)"_zv);

    work_.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID REFERENCES books(id) ON DELETE CASCADE,
    tag varchar(30) NOT NULL,
    PRIMARY KEY (book_id, tag)
);
)"_zv);

    work_.commit();
}

}  // namespace postgres