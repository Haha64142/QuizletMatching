vim.keymap.set("n", "<leader>b", function()
    vim.cmd "split | terminal cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && build/QuizletMatching"
end, { desc = "[B]uild Project" })
