# column-engine
Курсовая работа колоночный движок с нуля.

## Описание колоночного формат .BelZ:
Имеются следующие примитивы:
- `CSVReader` парсер CSV файлов
- `CSVWriter` писатель из колоночного формата в CSV 
- `BelZReader/BelZWriter` аналогичные классы но для колоночного формата
- `CSVConvertor` перегоняет файл .csv в файл .belz 
- `BelZConvertor` перегоняет файл .belz в файл .csv(для удобства к старому имени csv файла делается приписка upd)
- `Coder` применяет к колонкам dictionary encoding и bit packing
- `Decode` расжимает сжатые колонки при чтении файла .belz с диска

## Для запуска:
Перейти в column-engine/FileBasicTools/build, после чего есть два типа тестов:
- test_data_structures
- test_src 

`test_src` включает в себя довольно объёмный тест `bench.cpp`, который прогоняет первый миллион строк из hits.
