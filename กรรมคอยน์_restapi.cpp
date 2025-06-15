#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <ctime>
#include <mutex>
#include "httplib.h" // โหลด httplib.h มาด้วย

struct ประวัติภาวนา {
    std::time_t เวลา;
    std::string ลายเซ็น;
};

std::map<std::string, int> กระเป๋าเงิน;
std::map<std::string, std::vector<ประวัติภาวนา>> ฐานประวัติ;
std::mutex mtx; // ป้องกันข้อมูลขณะเข้าถึงพร้อมกัน

bool ตรวจสอบสมาธิ(const std::string& ผู้ใช้, const std::string& ลายเซ็น) {
    return ลายเซ็น == "ภาวนา";
}

int main() {
    httplib::Server svr;

    svr.Post("/claim", [&](const httplib::Request& req, httplib::Response& res) {
        auto user = req.get_param_value("user");
        auto signature = req.get_param_value("signature");

        if (user.empty() || signature.empty()) {
            res.status = 400;
            res.set_content("กรุณาระบุ user และ signature ให้ครบ", "text/plain; charset=utf-8");
            return;
        }

        if (!ตรวจสอบสมาธิ(user, signature)) {
            res.status = 403;
            res.set_content("สมาธิไม่ถูกต้อง ไม่สามารถรับเหรียญกรรมคอยน์ได้", "text/plain; charset=utf-8");
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mtx);
            กระเป๋าเงิน[user] += 1;
            ประวัติภาวนา บันทึก;
            บันทึก.เวลา = std::time(nullptr);
            บันทึก.ลายเซ็น = signature;
            ฐานประวัติ[user].push_back(บันทึก);
        }

        res.status = 200;
        res.set_content("ได้รับ กรรมคอยน์ 1 เหรียญแล้ว สาธุ!", "text/plain; charset=utf-8");
    });

    svr.Get("/balance", [&](const httplib::Request& req, httplib::Response& res) {
        auto user = req.get_param_value("user");
        if (user.empty()) {
            res.status = 400;
            res.set_content("กรุณาระบุ user", "text/plain; charset=utf-8");
            return;
        }
        int balance = 0;
        {
            std::lock_guard<std::mutex> lock(mtx);
            balance = กระเป๋าเงิน[user];
        }
        res.status = 200;
        res.set_content("ยอดเหรียญกรรมคอยน์ของ " + user + " = " + std::to_string(balance) + " เหรียญ", "text/plain; charset=utf-8");
    });

    svr.Get("/history", [&](const httplib::Request& req, httplib::Response& res) {
        auto user = req.get_param_value("user");
        if (user.empty()) {
            res.status = 400;
            res.set_content("กรุณาระบุ user", "text/plain; charset=utf-8");
            return;
        }
        std::string result = "ประวัติคำภาวนาของ " + user + ":\n";
        {
            std::lock_guard<std::mutex> lock(mtx);
            auto& ประวัติ = ฐานประวัติ[user];
            if (ประวัติ.empty()) {
                result += " ยังไม่มีประวัติคำภาวนา\n";
            } else {
                for (const auto& รายการ : ประวัติ) {
                    result += " - เวลา: " + std::string(std::ctime(&รายการ.เวลา));
                }
            }
        }
        res.status = 200;
        res.set_content(result, "text/plain; charset=utf-8");
    });

    std::cout << "กรรมคอยน์ REST API เริ่มทำงานที่พอร์ต 8080\n";
    svr.listen("0.0.0.0", 8080);

    return 0;
}