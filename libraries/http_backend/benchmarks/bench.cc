#include <li/http_backend/http_backend.hh>
#include <li/sql/mysql.hh>
#include <li/sql/pgsql.hh>

#include "symbols.hh"
using namespace li;

template <typename B>
void escape_html_entities(B& buffer, const std::string& data)
{
    for(size_t pos = 0; pos != data.size(); ++pos) {
        switch(data[pos]) {
            case '&':  buffer << "&amp;";       break;
            case '\"': buffer << "&quot;";      break;
            case '\'': buffer << "&apos;";      break;
            case '<':  buffer << "&lt;";        break;
            case '>':  buffer << "&gt;";        break;
            default:   buffer << data[pos]; break;
        }
    }
}

//#define PGSQL
#define BENCH_MYSQL

int main(int argc, char* argv[]) {

#ifdef BENCH_MYSQL
  auto sql_db =
      mysql_database(s::host = "127.0.0.1", s::database = "silicon_test", s::user = "root",
                     s::password = "sl_test_password", s::port = 14550, s::charset = "utf8");

#else
  auto sql_db = pgsql_database(s::host = "127.0.0.1", s::database = "postgres", s::user = "postgres",
                            s::password = "lithium_test", s::port = 32768, s::charset = "utf8");

#endif

  auto fortunes = sql_orm_schema(sql_db, "Fortune").fields(
    s::id(s::auto_increment, s::primary_key) = int(),
    s::message = std::string());

  auto random_numbers = sql_orm_schema(sql_db, "World").fields(
    s::id(s::auto_increment, s::primary_key) = int(),
    s::randomNumber = int());

    // { // init.

    //   auto c = random_numbers.connect();
    //   c.drop_table_if_exists().create_table_if_not_exists();
    //   for (int i = 0; i < 10000; i++)
    //     c.insert(s::randomNumber = i);
    //   auto f = fortunes.connect();
    //   f.drop_table_if_exists().create_table_if_not_exists();
    //   for (int i = 0; i < 100; i++)
    //     f.insert(s::message = "testmessagetestmessagetestmessagetestmessagetestmessagetestmessage");
    // }
  http_api my_api;

  my_api.get("/plaintext") = [&](http_request& request, http_response& response) {
    //response.set_header("Content-Type", "text/plain");
    response.write("Hello world!");
  };

  my_api.get("/json") = [&](http_request& request, http_response& response) {
    response.write_json(s::message = "Hello world!");
  };
  my_api.get("/db") = [&](http_request& request, http_response& response) {
    //std::cout << "start" << std::endl;
    response.write_json(random_numbers.connect(request.yield).find_one(s::id = 14).value());
    //std::cout << "end" << std::endl;
  };

  my_api.get("/queries") = [&](http_request& request, http_response& response) {
    std::string N_str = request.get_parameters(s::N = std::optional<std::string>()).N.value_or("1");
    //std::cout << N_str << std::endl;
    int N = atoi(N_str.c_str());
    
    N = std::max(1, std::min(N, 500));
    
    auto c = random_numbers.connect(request.yield);
    auto& raw_c = c.backend_connection();
    //raw_c("START TRANSACTION");
    std::vector<decltype(random_numbers.all_fields())> numbers(N);
    //auto stm = c.backend_connection().prepare("SELECT randomNumber from World where id=$1");
    for (int i = 0; i < N; i++)
      numbers[i] = c.find_one(s::id = 1 + rand() % 99).value();
      //numbers[i] = stm(1 + rand() % 99).read<std::remove_reference_t<decltype(numbers[i])>>();
      //numbers[i].randomNumber = stm(1 + rand() % 99).read<int>();
    //raw_c("COMMIT");

    response.write_json(numbers);
  };

  my_api.get("/updates") = [&](http_request& request, http_response& response) {
    // try {
    // std::cout << "up" << std::endl;  
    std::string N_str = request.get_parameters(s::N = std::optional<std::string>()).N.value_or("1");
    int N = atoi(N_str.c_str());
    N = std::max(1, std::min(N, 500));
    
    auto c = random_numbers.connect(request.yield);
    auto& raw_c = c.backend_connection();
    std::vector<decltype(random_numbers.all_fields())> numbers(N);
    
    // raw_c("START TRANSACTION ISOLATION LEVEL REPEATABLE READ");
#ifdef BENCH_MYSQL
    raw_c("START TRANSACTION");
#endif

    for (int i = 0; i < N; i++)
    {
      numbers[i] = c.find_one(s::id = 1 + rand() % 9999).value();
      numbers[i].randomNumber = 1 + rand() % 9999;
    }

    std::sort(numbers.begin(), numbers.end(), [] (auto a, auto b) { return a.id < b.id; });
    
#ifdef BENCH_MYSQL
    for (int i = 0; i < N; i++)
      c.update(numbers[i]);
#else
    // std::ostringstream ss_lock;
    // ss_lock << "select * from World where id in ("; 
    // for (int i = 0; i < N; i++)
    //   ss_lock << numbers[i].id << (i == N-1 ? "": ",");
    // ss_lock << ") for update";
    // // std::cout << ss_lock.str() << std::endl; 
    // raw_c(ss_lock.str());

    // for (int i = 0; i < N; i++)
    //   c.update(numbers[i]);

    // std::ostringstream ss;
    // ss << "UPDATE World SET randomNumber=tmp.randomNumber FROM (VALUES ";
    // for (int i = 0; i < N; i++)
    //   ss << "(" << numbers[i].id << ", " << numbers[i].randomNumber << ") "<< (i == N-1 ? "": ",");
    //   //ss << "(" << numbers[i].id << ", " << numbers[i].randomNumber << ") "<< (i == N-1 ? "": ",");
    // ss << " ORDER BY 1) AS tmp(id, randomNumber) WHERE tmp.id = World.id";
    // // std::cout << ss.str() << std::endl;
    // raw_c(ss.str());


    // auto get_statement = [&] (){ 
    //   if (!raw_c.has_cached_statement(s::bulk_insert))
    //   {
    //     std::ostringstream ss;
    //     ss << "UPDATE World SET randomNumber=tmp.randomNumber FROM (VALUES ";
    //     for (int i = 0; i < N; i++)
    //       ss << "($" << i*2+1 << "::integer, $" << i*2+2 << "::integer) "<< (i == N-1 ? "": ",");
    //     ss << ") AS tmp(id, randomNumber) WHERE tmp.id = World.id";
    //     auto stmt = raw_c.prepare(ss.str());
    //     raw_c.cache_statement(stmt, s::bulk_insert);
    //     return stmt;
    //   }
    //   else return raw_c.get_cached_statement(s::bulk_insert);
    // };

    //std::cout << ss.str() << std::endl;
    raw_c.cached_statement([N] {
        std::ostringstream ss;
        ss << "UPDATE World SET randomNumber=tmp.randomNumber FROM (VALUES ";
        for (int i = 0; i < N; i++)
          ss << "($" << i*2+1 << "::integer, $" << i*2+2 << "::integer) "<< (i == N-1 ? "": ",");
        ss << ") AS tmp(id, randomNumber) WHERE tmp.id = World.id";
        return ss.str();
    }, N)(numbers);
    // (
    //   numbers[0].id, numbers[0].randomNumber,
    //   numbers[1].id, numbers[1].randomNumber,
    //   numbers[2].id, numbers[2].randomNumber,
    //   numbers[3].id, numbers[3].randomNumber,
    //   numbers[4].id, numbers[4].randomNumber,
    //   numbers[5].id, numbers[5].randomNumber,
    //   numbers[6].id, numbers[6].randomNumber,
    //   numbers[7].id, numbers[7].randomNumber,
    //   numbers[8].id, numbers[8].randomNumber,
    //   numbers[9].id, numbers[9].randomNumber,

    //   numbers[10].id, numbers[10].randomNumber,
    //   numbers[11].id, numbers[11].randomNumber,
    //   numbers[12].id, numbers[12].randomNumber,
    //   numbers[13].id, numbers[13].randomNumber,
    //   numbers[14].id, numbers[14].randomNumber,
    //   numbers[15].id, numbers[15].randomNumber,
    //   numbers[16].id, numbers[16].randomNumber,
    //   numbers[17].id, numbers[17].randomNumber,
    //   numbers[18].id, numbers[18].randomNumber,
    //   numbers[19].id, numbers[19].randomNumber
    // );

    // std::ostringstream ss;
    // ss << "UPDATE World SET randomNumber=case id ";
    // for (int i = 0; i < N; i++)
    //   ss << " when " << numbers[i].id << " then " << numbers[i].randomNumber;
    // ss << " else randomNumber end where id in (";
    // for (int i = 0; i < N; i++)
    //   ss << numbers[i].id << (i == N-1 ? "": ",");
    // ss << ")";
    // //std::cout << ss.str() << std::endl;
    // raw_c(ss.str());

#endif
#ifdef BENCH_MYSQL
    raw_c("COMMIT");
#endif

    // std::cout << raw_c.prepare("select randomNumber from World where id=$1")(numbers[0].id).read<int>() << " " << numbers[0].randomNumber << std::endl;   
    response.write_json(numbers);
  };

  my_api.get("/fortunes") = [&](http_request& request, http_response& response) {

    typedef decltype(fortunes.all_fields()) fortune;

    std::vector<fortune> table;

    auto c = fortunes.connect(request.yield);
    c.forall([&] (auto f) { table.emplace_back(std::move(f)); });
    table.emplace_back(0, std::string("Additional fortune added at request time."));

    std::sort(table.begin(), table.end(),
              [] (const fortune& a, const fortune& b) { return a.message < b.message; });

    char b[100000];
    li::output_buffer ss(b, sizeof(b));
    ss << "<!DOCTYPE html><html><head><title>Fortunes</title></head><body><table><tr><th>id</th><th>message</th></tr>";
    for(auto& f : table)
    {
      ss << "<tr><td>" << f.id << "</td><td>";
      escape_html_entities(ss, f.message); 
      ss << "</td></tr>";
    }
    ss << "</table></body></html>";

    response.set_header("Content-Type", "text/html; charset=utf-8");
    response.write(ss.to_string_view());
  };

#ifdef BENCH_MYSQL  
  int mysql_max_connection = sql_db.connect()("SELECT @@GLOBAL.max_connections;").read<int>() * 0.75;
  std::cout << "mysql max connection " << mysql_max_connection << std::endl;
  int port = atoi(argv[1]);
  // int port = 12667;
  int nthread = 4;
  li::max_mysql_connections_per_thread = (mysql_max_connection / nthread);

#else
  int mysql_max_connection = atoi(sql_db.connect()("SHOW max_connections;").read<std::string>().c_str()) * 0.75;
  std::cout << "sql max connection " << mysql_max_connection << std::endl;
  int port = atoi(argv[1]);
  // int port = 12543;
  
  int nthread = 4;
  li::max_pgsql_connections_per_thread = (mysql_max_connection / nthread);
#endif

  http_serve(my_api, port, s::nthreads = nthread); 
  
  return 0;

}
