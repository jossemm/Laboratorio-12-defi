#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <iomanip>
#include <curl/curl.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

using namespace std;

string trim(const string& str) {
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    return (start == string::npos || end == string::npos) ? "" : str.substr(start, end - start + 1);
}

void cargarConocimiento(map<string, string>& conocimiento, const string& nombreArchivo) {
    ifstream archivo(nombreArchivo);
    if (!archivo.is_open()) {
        cout << "Error al abrir el archivo de conocimiento." << endl;
        return;
    }

    string linea;
    while (getline(archivo, linea)) {
        size_t separador = linea.find("|");
        if (separador != string::npos) {
            string pregunta = trim(linea.substr(0, separador));
            string respuesta = trim(linea.substr(separador + 1));
            conocimiento[pregunta] = respuesta;
        }
    }
    archivo.close();
}

string buscarExacto(const map<string, string>& conocimiento, const string& pregunta) {
    string preguntaLower = pregunta;
    transform(preguntaLower.begin(), preguntaLower.end(), preguntaLower.begin(), ::tolower);

    for (const auto& par : conocimiento) {
        string claveLower = par.first;
        transform(claveLower.begin(), claveLower.end(), claveLower.begin(), ::tolower);

        if (preguntaLower == claveLower) {
            return par.second;
        }
    }
    return "";
}

vector<string> dividirPalabras(const string& frase) {
    vector<string> palabras;
    stringstream ss(frase);
    string palabra;
    while (ss >> palabra) {
        transform(palabra.begin(), palabra.end(), palabra.begin(), ::tolower);
        palabras.push_back(palabra);
    }
    return palabras;
}

string buscarPorPalabrasClave(const map<string, string>& conocimiento, const string& pregunta) {
    vector<string> palabrasPregunta = dividirPalabras(pregunta);
    string mejorRespuesta = "";
    int maxCoincidencias = 0;

    for (const auto& par : conocimiento) {
        vector<string> palabrasBase = dividirPalabras(par.first);

        int coincidencias = 0;
        for (const string& palabraPregunta : palabrasPregunta) {
            for (const string& palabraBase : palabrasBase) {
                if (palabraPregunta == palabraBase) {
                    coincidencias++;
                }
            }
        }

        if (coincidencias > maxCoincidencias) {
            maxCoincidencias = coincidencias;
            mejorRespuesta = par.second;
        }
    }
    return (maxCoincidencias > 0) ? mejorRespuesta : "";
}

void buscarCoincidencias(const char* archivo, const char* palabraClave) {
    ifstream file(archivo);
    if (!file.is_open()) {
        cerr << "Error abriendo el archivo" << endl;
        return;
    }

    string linea;
    int numeroLinea = 1;

    while (getline(file, linea)) {
        if (linea.find(palabraClave) != string::npos) {
            cout << "Coincidencia en línea " << numeroLinea << ": " << linea << endl;
        }
        numeroLinea++;
    }
    file.close();
}

struct RespuestaAPI {
    char* datos;
    size_t tamano;
};

static size_t escribirRespuesta(void* contenido, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    RespuestaAPI* resp = static_cast<RespuestaAPI*>(userp);

    char* ptr = reinterpret_cast<char*>(realloc(resp->datos, resp->tamano + total + 1));
    if (!ptr) return 0;

    resp->datos = ptr;
    memcpy(&(resp->datos[resp->tamano]), contenido, total);
    resp->tamano += total;
    resp->datos[resp->tamano] = '\0';
    return total;
}

string escape_json(const string& s) {
    ostringstream o;
    for (auto c = s.cbegin(); c != s.cend(); c++) {
        switch (*c) {
        case '"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\b': o << "\\b"; break;
        case '\f': o << "\\f"; break;
        case '\n': o << "\\n"; break;
        case '\r': o << "\\r"; break;
        case '\t': o << "\\t"; break;
        default:
            if ('\x00' <= *c && *c <= '\x1f') {
                o << "\\u" << hex << setw(4) << setfill('0') << (int)*c;
            }
            else {
                o << *c;
            }
        }
    }
    return o.str();
}

string consultarOpenAI(const string& pregunta) {
    CURL* curl;
    CURLcode res;
    RespuestaAPI respuesta = { nullptr, 0 };

    const string api_key = "Aqui El api key";
    if (api_key == "Bearer sk-tu-api-key-aqui") {
        return "ERROR: Por favor configura tu API key de OpenAI en el código (reemplaza 'sk-tu-api-key-aqui')";
    }

    const string url = "https://api.openai.com/v1/chat/completions";
    string json = "{"
        "\"model\": \"gpt-3.5-turbo\","
        "\"messages\": [{\"role\": \"user\", \"content\": \"" + escape_json(pregunta) + "\"}],"
        "\"temperature\": 0.7"
        "}";

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: " + api_key).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, escribirRespuesta);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(&respuesta));
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        cout << "\nJSON enviado:\n" << json << endl;
        res = curl_easy_perform(curl);

        string resultado;
        if (res != CURLE_OK) {
            resultado = "Error al conectar con OpenAI: " + string(curl_easy_strerror(res));
        }
        else if (respuesta.datos) {
            string respuesta_json = respuesta.datos;

            size_t contenido_pos = respuesta_json.find("\"content\":\"");
            if (contenido_pos != string::npos) {
                size_t inicio = contenido_pos + 10;
                size_t fin = respuesta_json.find("\"", inicio);
                if (fin != string::npos) {
                    resultado = respuesta_json.substr(inicio, fin - inicio);
                    size_t pos = 0;
                    while ((pos = resultado.find("\\n", pos)) != string::npos) {
                        resultado.replace(pos, 2, "\n");
                        pos += 1;
                    }
                    pos = 0;
                    while ((pos = resultado.find("\\\"", pos)) != string::npos) {
                        resultado.replace(pos, 2, "\"");
                        pos += 1;
                    }
                }
                else {
                    resultado = "Error: No se pudo encontrar el fin de la respuesta en el JSON";
                }
            }
            else {
                resultado = "Respuesta completa de OpenAI:\n" + respuesta_json;
            }
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(respuesta.datos);
        curl_global_cleanup();

        return resultado;
    }
    return "Error al inicializar cURL";
}

int main() {
    setlocale(LC_ALL, "es_ES.UTF-8");

    map<string, string> conocimiento;
    cargarConocimiento(conocimiento, "conocimiento.txt");

    cout << "=== Chatbot Inteligente ===" << endl;
    cout << "Opciones especiales:" << endl;
    cout << " - 'buscar [palabra]': Busca en archivos" << endl;
    cout << " - 'openai [pregunta]': Consulta a OpenAI (extra)" << endl;
    cout << " - 'adios': Terminar el programa" << endl << endl;

    string input;
    while (true) {
        cout << "\nTu: ";
        getline(cin, input);

        if (input == "adios") {
            cout << "Bot: Hasta luego!" << endl;
            break;
        }

        if (input.rfind("buscar ", 0) == 0) {
            string palabra = input.substr(7);
            cout << "=== Resultados de búsqueda ===" << endl;
            buscarCoincidencias("conocimiento.txt", palabra.c_str());
            continue;
        }

        if (input.rfind("openai ", 0) == 0) {
            string pregunta = input.substr(7);
            cout << "Consultando a OpenAI...\n";
            string respuesta = consultarOpenAI(pregunta);
            cout << "Bot (OpenAI): " << respuesta << endl;
            continue;
        }

        string respuesta = buscarExacto(conocimiento, input);
        if (respuesta.empty()) {
            respuesta = buscarPorPalabrasClave(conocimiento, input);
        }

        if (respuesta.empty()) {
            respuesta = "No sé la respuesta. ¿Quieres intentar con 'openai [tu pregunta]'?";
        }

        cout << "Bot: " << respuesta << endl;
    }

    return 0;
}